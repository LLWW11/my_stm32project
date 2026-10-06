#include "dbg_config.h"
#include "25q128/25q128.h"
#include "can_iap.h"
#include "task.h"
#include <string.h>
#include <stddef.h> /* offsetof */
#include "boot_crc.h"
#include "boot_Image_desc.h" /* 镜像格式合 */

/*

约定can帧格式：
    全部使用标准帧，小端序
 报文             | DLC     | Data布局(字节数组)
 启动 0x120       | 8       | [0..3] BIN 长度；[4..7] BIN 的 CRC32
 数据 0x121       | 5-8     | [0] 会话号；[1..3] BIN 偏移；[4..] 本帧 1～4 字节固件
 终止 0x122       | 5       | [0] 会话号；[1..4] BIN 长度
 回复 0x123       | 8       | [0] 状态；[1] 会话号；[2..4] 下一期望偏移；[5] 错误码；[6] 请求 ID 低字节；[7] 0xA5
 查询与版本帧暂未实现，本任务目前只接受 START、DATA、END

*/

#define IAP_ID_START 0x120U
#define IAP_ID_DATA  0x121U
#define IAP_ID_END   0x122U
#define IAP_ID_REPLY 0x123U

/* W25Q128 布局、magic、state、APP 分区等常量见 boot_Image_desc.h，此处不再重复定义 */

#define IAP_GAP_TIMEOUT_MS 3000 // 传输中两帧最大间隔
#define IAP_CRC_CHUNK      256  // 分片CRC校验缓冲区

#define IAP_ACK  0x00 // 回复帧[0]状态
#define IAP_NACK 0x01
static bool s_iap_resume_ok = false; // AT24C02 可用才启用续传
typedef enum
{
    IAP_ERR_OK = 0,
    IAP_ERR_FRAME,
    IAP_ERR_LENGTH,
    IAP_ERR_OFFSET,
    IAP_ERR_SESSION,
    IAP_ERR_FLASH, // 写入W25Q128错误
    IAP_ERR_CRC,   // CRC错误
    IAP_ERR_TIMEOUT
} iap_err_t;

typedef enum
{
    IAP_IDLE = 0,
    IAP_RECEIVING
} iap_state_t;

typedef struct
{
    iap_state_t state;
    uint8_t session; // 会话号：由 START 后首个合法顺序 DATA 锁定；仅保存在 RAM
    uint8_t session_valid;
    uint32_t image_len; // START 下发的 BIN 长度
    uint32_t image_crc; // START 下发的全量 CRC32
    uint32_t expected;  // 下一期望偏移
    uint32_t persisted; // 已写进 AT24C02 的偏移
} iap_ctx_t;

typedef struct
{
    uint32_t magic;     // IAP_RR_MAGIC
    uint32_t image_len; // START 下发的长度
    uint32_t image_crc; // START 下发的 CRC32
    uint32_t expected;  // 已确认落盘的偏移 = 断点
    uint8_t state;      //
    uint8_t reserved[3];
    uint32_t record_crc; // 覆盖前 20 字节
} iap_resume_rec_t;

// 编译期防护：字段错位/被填充时直接编译失败
typedef char iap_resume_rec_size_check[(sizeof(iap_resume_rec_t) == 24U) ? 1 : -1];

// 进度写到at24c02
static bool iap_resume_store(const iap_resume_rec_t *rec)
{
    iap_resume_rec_t tmp = *rec;
    tmp.magic = IAP_RR_MAGIC;
    tmp.record_crc = boot_crc32((const uint8_t *)&tmp,
                                (uint32_t)offsetof(iap_resume_rec_t, record_crc));
    return AT24C02_WriteBuffer(IAP_RR_ADDR, (const uint8_t *)&tmp,
                               (uint32_t)sizeof(tmp)) == AT24C02_OK;
}

// 读出记录并校验有效性
static bool iap_resume_load(iap_resume_rec_t *rec)
{
    if (AT24C02_ReadBuffer(IAP_RR_ADDR, (uint8_t *)rec,
                           (uint32_t)sizeof(*rec)) != AT24C02_OK)
        return false;
    if (rec->magic != IAP_RR_MAGIC)
        return false;
    return rec->record_crc == boot_crc32((const uint8_t *)rec,
                                         (uint32_t)offsetof(iap_resume_rec_t, record_crc));
}
// maigc写成0xffffffff，下次读出必然无效
static void iap_resume_invalidate(void)
{
    iap_resume_rec_t rec;
    if (!s_iap_resume_ok)
        return;
    memset(&rec, 0xFF, sizeof(rec));
    (void)AT24C02_WriteBuffer(IAP_RR_ADDR, (const uint8_t *)&rec, (uint32_t)sizeof(rec));
}
// 保存当前进度
static bool iap_resume_save_progress(const iap_ctx_t *ctx)
{
    iap_resume_rec_t rec = {0};
    rec.image_len = ctx->image_len;
    rec.image_crc = ctx->image_crc;
    rec.expected = ctx->expected;
    rec.state = IAP_RR_STATE_RECEIVING;
    return iap_resume_store(&rec);
}
// 头部若已 READY/DONE，说明上次传输已提交，续传记录无意义
static bool iap_w25_header_committed(void)
{
    boot_image_header_t h;
    if (W25Q_Read(BOOT_IMAGE_W25_HEADER, (uint8_t *)&h, sizeof(h)) != W25Q_OK)
        return false;
    if (h.magic != BOOT_IMAGE_MAGIC)
        return false;
    return (h.state == BOOT_IMAGE_STATE_READY) ||
           (h.state == BOOT_IMAGE_STATE_DONE);
}
// 从协议字节数组读取小端 32 位无符号数
static uint32_t IAP_Get_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
// 从 DATA 偏移字段读取小端 24 位无符号数
static uint32_t IAP_Get_u24_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16);
}

// 发布一帧0x123: 状态+会话+下一期望偏移+错误码
static bool iap_reply(uint8_t status,
                      uint8_t session,
                      uint32_t next_off,
                      iap_err_t err,
                      uint8_t request_id)
{
    uint8_t dat[8];
    dat[0] = status;  // 第一个字节表示状态
    dat[1] = session; // 会话号，防止CAN传输过程中断电后不知道从哪里续传
    dat[2] = (uint8_t)next_off;
    dat[3] = (uint8_t)(next_off >> 8);
    dat[4] = (uint8_t)(next_off >> 16);
    dat[5] = (uint8_t)err;
    dat[6] = request_id; // 0x20 START、0x21 DATA、0x22 END；0 表示主动超时
    dat[7] = 0xA5U;      // 回复格式标记，使主机能识别旧 APP 的无标签回复
    return can_test_send(IAP_ID_REPLY, dat, 8, pdMS_TO_TICKS(100));
}
// 收到 0x120：擦除头部扇区与正文覆盖的所有扇区，全部成功才回 ACK
static iap_err_t iap_handle_start(const uint8_t *d, uint8_t dlc, iap_ctx_t *ctx)
{
    uint32_t len, crc, addr, end;
    iap_resume_rec_t rec;
    bool resume = false; // 接收过程是否被打断
    if (dlc != 8)
        return IAP_ERR_FRAME;

    len = IAP_Get_u32_le(&d[0]);
    crc = IAP_Get_u32_le(&d[4]);
    if ((len < 8) || (len > BOOT_IMAGE_APP_MAX_SIZE))
        return IAP_ERR_LENGTH;
    if (ctx->state == IAP_RECEIVING) // 活跃传输期间拒绝重复 START，避免误擦已接收数据
        return IAP_ERR_FRAME;

    // 记录命中：magic/CRC 有效、长度与 CRC 都对上、上次接收被打断
    if (s_iap_resume_ok && iap_resume_load(&rec) &&
        (rec.image_len == len) && (rec.image_crc == crc) &&
        (rec.state == IAP_RR_STATE_RECEIVING) &&
        (rec.expected <= len) && !iap_w25_header_committed())
        resume = true;

    memset(ctx, 0, sizeof(*ctx));
    ctx->image_len = len;
    ctx->image_crc = crc;

    if (resume) // 续传，不擦除头部
    {
        ctx->state = IAP_RECEIVING;
        ctx->expected = rec.expected;
        ctx->persisted = rec.expected;
        ctx->session_valid = 0; // 会话在续传后首个 DATA 上重绑
        printf("[IAP] RESUME from %lu / %lu\r\n",
               (unsigned long)rec.expected, (unsigned long)len);
        return IAP_ERR_OK;
    }
    else // 如果不是续传,重新开始
    {
        if (s_iap_resume_ok)
        {
            iap_resume_rec_t er;
            memset(&er, 0, sizeof(er));
            er.image_len = len;
            er.image_crc = crc;
            er.state = IAP_RR_STATE_ERASING;
            (void)iap_resume_store(&er);
        }
        printf("[W25Q128] START len=%lu crc=%08lX, erasing...\r\n",
               (unsigned long)len, (unsigned long)crc);
        if (W25Q_EraseSector(BOOT_IMAGE_W25_HEADER) != W25Q_OK)
            return IAP_ERR_FLASH;

        end = BOOT_IMAGE_W25_BODY + len;
        for (addr = BOOT_IMAGE_W25_BODY; addr < end; addr += W25Q128_SECTOR_SIZE)
        {
            if (W25Q_EraseSector(addr) != W25Q_OK)
                return IAP_ERR_FLASH;
        }
        ctx->state = IAP_RECEIVING;
        ctx->image_len = len;
        ctx->image_crc = crc;
        ctx->expected = 0;
        ctx->session_valid = 0;

        printf("[W25Q128] Erase done, waiting DATA\r\n");
        return IAP_ERR_OK;
    }
}

// 收到 0x121：偏移必须衔接 expected，只写新字节；
static iap_err_t iap_handle_data(const uint8_t *d, uint8_t dlc, iap_ctx_t *ctx)
{
    uint32_t offset;
    uint8_t p_Len;
    if ((dlc < 5) || (dlc > 8)) //
        return IAP_ERR_FRAME;
    p_Len = dlc - 4; // 前4字节是会话号+写入的地址偏移
    if (ctx->session_valid && ctx->session != d[0])
        return IAP_ERR_SESSION;

    offset = IAP_Get_u24_le(&d[1]); // 要写入W25Q128的bin的偏移地址

    if (offset + (uint32_t)p_Len > ctx->image_len)
        return IAP_ERR_LENGTH; // 超出长度了，要么是地址偏移出错，要么是已经写完还有新帧

    if (offset + (uint32_t)p_Len <= ctx->expected)
    {
        // 正文已收齐后的续传：主机重发末尾分片，以便绑定新会话再发送 END
        if (!ctx->session_valid && ctx->expected == ctx->image_len &&
            offset + (uint32_t)p_Len == ctx->expected)
        {
            uint8_t tail[4];
            if (W25Q_Read(BOOT_IMAGE_W25_BODY + offset, tail, p_Len) != W25Q_OK)
                return IAP_ERR_FLASH;
            if (memcmp(tail, &d[4], p_Len) != 0)
            {
                iap_resume_invalidate(); // 断点尾部损坏，下次 START 必须重新擦除
                return IAP_ERR_CRC;
            }
            ctx->session = d[0];
            ctx->session_valid = 1;
        }
        return IAP_ERR_OK; // 完全重复帧：早已写入，回进度即可
    }

    if (offset > ctx->expected)
        return IAP_ERR_OFFSET; // 前面有空洞：NACK，让主机从expected重发

    // 只在首个合法顺序 DATA 上绑定会话，畸形首帧不能抢占会话
    if (ctx->session_valid == 0)
    {
        ctx->session = d[0];
        ctx->session_valid = 1;
    }

    if (offset == ctx->expected) // 正常顺序帧：整帧写入
    {
        if (W25Q_Write(BOOT_IMAGE_W25_BODY + ctx->expected, &d[4], p_Len) != W25Q_OK)
            return IAP_ERR_FLASH;
    }
    else // 部分重叠：NOR 只能 1->0，跳过已写部分，只补空洞之后的新字节
    {
        uint32_t holeSkip = ctx->expected - offset;
        if (W25Q_Write(BOOT_IMAGE_W25_BODY + ctx->expected,
                       &d[4 + holeSkip], p_Len - holeSkip) != W25Q_OK)
            return IAP_ERR_FLASH;
    }
    ctx->expected = offset + p_Len; // 两条写入路径都必须推进期望偏移
    // 每 4KB 或收齐时记录一次进度；数据已写在前，persisted 只落后不超前
    if (s_iap_resume_ok && (((ctx->expected - ctx->persisted) >= IAP_CKPT_INTERVAL) ||
                            (ctx->expected == ctx->image_len)))
    {
        if (iap_resume_save_progress(ctx))
            ctx->persisted = ctx->expected;
        else
            printf("[IAP] resume record write FAIL @%lu\r\n",
                   (unsigned long)ctx->expected); // 只降级，不中断传输
    }

    if ((ctx->expected & 0xFFFU) == 0) // 每 4KB 打印一次
        printf("[W25Q128] Writing firmware: %lu / %lu\r",
               (unsigned long)ctx->expected, (unsigned long)ctx->image_len);
    return IAP_ERR_OK;
}

// 收到 0x122：收齐 -> 分块CRC -> 回写头部 -> 置状态位
static iap_err_t iap_handle_end(const uint8_t *d, uint8_t dlc, iap_ctx_t *ctx)
{
    boot_image_header_t header; //
    uint8_t buf[IAP_CRC_CHUNK];
    uint32_t len, off, n, calc;

    if (dlc != 5)
        return IAP_ERR_FRAME;
    if ((d[0] != ctx->session) || ctx->session_valid == 0)
        return IAP_ERR_SESSION;

    len = IAP_Get_u32_le(&d[1]);

    if (len != ctx->image_len)
        return IAP_ERR_LENGTH; // 长度都不对了肯定不合法
    if (ctx->expected != ctx->image_len)
        return IAP_ERR_OFFSET; // 还没收齐
    printf("[W25Q128] Bin was written into W25Q128, verifying CRC...\r\n");

    // 分块CRC校验
    calc = 0xFFFFFFFFU;
    for (off = 0; off < len; off += n)
    {
        n = len - off; // 先算本块长度
        if (n > IAP_CRC_CHUNK)
            n = IAP_CRC_CHUNK;
        if (W25Q_Read(BOOT_IMAGE_W25_BODY + off, buf, n) != W25Q_OK)
            return IAP_ERR_FLASH;
        calc = boot_crc32_update(calc, buf, n);
    }
    calc ^= 0xFFFFFFFFU;
    if (calc != ctx->image_crc)
    {
        printf("[W25Q128] CRC FAIL got=%08lX expected=%08lX\r\n",
               (unsigned long)calc, (unsigned long)ctx->image_crc);
        iap_resume_invalidate(); // 正文已坏，续传记录没意义，下次全量重来
        return IAP_ERR_CRC;
    }
    // CRC 正确也可能是地址不匹配的 BIN；提交前按 Bootloader 规则检查向量
    if (W25Q_Read(BOOT_IMAGE_W25_BODY, buf, 8U) != W25Q_OK)
        return IAP_ERR_FLASH;
    {
        uint32_t msp = IAP_Get_u32_le(buf);
        uint32_t reset = IAP_Get_u32_le(buf + 4U);
        uint32_t entry = reset & ~1U;
        if ((msp < 0x20000000U) || (msp > 0x20020000U) ||
            ((msp & 7U) != 0U) || ((reset & 1U) == 0U) ||
            (entry < BOOT_IMAGE_APP_BASE + 8U) ||
            (entry >= BOOT_IMAGE_APP_BASE + len))
        {
            iap_resume_invalidate();
            return IAP_ERR_LENGTH;
        }
    }
    printf("[W25Q128] CRC PASS, writing header\r\n");
    memset(&header, 0xFF, sizeof(header)); // state 不赋值，保持擦除态 0xFFFFFFFF
    header.magic = BOOT_IMAGE_MAGIC;
    header.target = BOOT_IMAGE_APP_BASE;
    header.length = ctx->image_len;
    header.image_crc = ctx->image_crc;
    // CRC 覆盖开头到 header_crc 之前
    header.header_crc = boot_crc32((const uint8_t *)&header, offsetof(boot_image_header_t, header_crc));

    // 写入整个头部：state 位置写 0xFFFFFFFF，对 NOR 相当于什么都没写
    if (W25Q_Write(BOOT_IMAGE_W25_HEADER,
                   (const uint8_t *)&header, sizeof(header)) != W25Q_OK)
        return IAP_ERR_FLASH;

    // READY 提交之前验证完整头部，避免带着错误元数据进入可安装态
    if (W25Q_Read(BOOT_IMAGE_W25_HEADER, buf, sizeof(header)) != W25Q_OK ||
        memcmp(buf, &header, sizeof(header)) != 0)
        return IAP_ERR_FLASH;

    // 提交动作——单独把 state 从 0xFFFFFFFF 编程为 READY
    //    只清 bit0 合法，无需擦除；掉电安全：没走到这步 state 永远是 0xFF
    header.state = BOOT_IMAGE_STATE_READY;
    if (W25Q_Write(BOOT_IMAGE_W25_HEADER + offsetof(boot_image_header_t, state),
                   (const uint8_t *)&header.state, sizeof(header.state)) != W25Q_OK)
        return IAP_ERR_FLASH;

    // 写完回读整个头部，逐字段验收，确认 bootloader 能读到合法头部
    memset(&header, 0, sizeof(header));
    if (W25Q_Read(BOOT_IMAGE_W25_HEADER,
                  (uint8_t *)&header, sizeof(header)) != W25Q_OK)
        return IAP_ERR_FLASH;
    if ((header.magic != BOOT_IMAGE_MAGIC) ||
        (header.target != BOOT_IMAGE_APP_BASE) ||
        (header.length != ctx->image_len) ||
        (header.image_crc != ctx->image_crc) ||
        (header.state != BOOT_IMAGE_STATE_READY) ||
        (header.header_crc != boot_crc32((const uint8_t *)&header,
                                         offsetof(boot_image_header_t, header_crc))))
        return IAP_ERR_FLASH;
    iap_resume_invalidate(); // 已经提交，作废记录
    ctx->state = IAP_IDLE;
    printf("[IAP] image READY, rebooting\r\n");
    return IAP_ERR_OK;
}

// 消费CAN中断接收队列，执行 START/DATA/END
void can_iap_task(void *argument)
{
    iap_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    CanRxMsg rx;
    uint32_t id;
    iap_err_t err;

    (void)argument;

    if (!can_test_init(false))
    {
        printf("[IAP] CAN init failed\r\n");
        vTaskDelete(NULL);
    }

    if ((W25Q_ReadId(&id) != W25Q_OK) || (id != W25Q128_JEDEC_ID))
    {
        printf("[IAP] W25 check FAIL\r\n");
        vTaskDelete(NULL);
        return;
    }
    else
        printf("[IAP] task ready, W25 OK\r\n");

    s_iap_resume_ok = (AT24C02_Init() == AT24C02_OK);
    printf("[IAP] AT24C02 %s, resume %s\r\n",
           s_iap_resume_ok ? "OK" : "missing",
           s_iap_resume_ok ? "enabled" : "disabled");
    if (!s_iap_resume_ok)
        printf("[IAP] dbg BUSY=%d SCL(PB8)=%d SDA(PB9)=%d\r\n",
               (int)I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY),
               (int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8),
               (int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9));

    while (1)
    {
        TickType_t timeout = (ctx.state == IAP_RECEIVING)
                                 ? pdMS_TO_TICKS(IAP_GAP_TIMEOUT_MS)
                                 : portMAX_DELAY;
        if (can_test_receive(&rx, timeout) == false) // 超时
        {
            if (ctx.state == IAP_RECEIVING)
            {
                printf("[IAP] CAN RX timeout\r\n");
                ctx.state = IAP_IDLE;
                iap_reply(IAP_NACK, ctx.session, ctx.expected, IAP_ERR_TIMEOUT, 0U);
            }
            continue;
        }

        err = IAP_ERR_FRAME;
        switch (rx.StdId)
        {
        case IAP_ID_START:
            if (rx.DLC == 8)
                err = iap_handle_start(rx.Data, rx.DLC, &ctx);
            break;
        case IAP_ID_DATA:
            if (ctx.state == IAP_RECEIVING && rx.DLC >= 5 && rx.DLC <= 8)
                err = iap_handle_data(rx.Data, rx.DLC, &ctx);
            break;
        case IAP_ID_END:
            if (ctx.state == IAP_RECEIVING && rx.DLC == 5)
                err = iap_handle_end(rx.Data, rx.DLC, &ctx);
            break;
        default:
            continue; // 滤波器外的 ID直接丢弃
        }
        // END 成功：回 ACK，给主机 500ms 收到的时间，然后复位进 Bootloader
        if ((err == IAP_ERR_OK) && (rx.StdId == IAP_ID_END))
        {
            iap_reply(IAP_ACK, ctx.session, ctx.expected, IAP_ERR_OK, (uint8_t)rx.StdId);
            vTaskDelay(pdMS_TO_TICKS(500));
            NVIC_SystemReset();
        }
        // 没有成功
        // START 尚未绑定会话：固定回复会话 0；DATA/END 回显请求会话
        iap_reply((err == IAP_ERR_OK) ? IAP_ACK : IAP_NACK,
                  (rx.StdId == IAP_ID_START) ? 0U : rx.Data[0], ctx.expected, err,
                  (uint8_t)rx.StdId);
        if ((err != IAP_ERR_OK) && (ctx.state == IAP_RECEIVING) &&
            ((err == IAP_ERR_LENGTH) || (err == IAP_ERR_FLASH) || (err == IAP_ERR_CRC)))
        {
            printf("[IAP] fatal err=%d, back to IDLE\r\n", (int)err);
            ctx.state = IAP_IDLE;
        }
    }
}

// loopback与双机测试的任务
void can_test_task(void *argument)
{
#define CAN_TEST_LOOPBACK 0
    CanRxMsg rx;
#if CAN_TEST_LOOPBACK
    const uint8_t probe[4] = {0x4C, 0x4F, 0x4F, 0x50}; // "LOOP"
#endif
    uint8_t seq = 0;

    (void)argument;

    if (!can_test_init(CAN_TEST_LOOPBACK != 0))
    {
        printf("CAN init failed\r\n");
        vTaskDelete(NULL);
    }
    can_test_enable_probe_filter(); // 双节点正常模式同样需要接收 0x321 测试报文

    while (1)
    {
#if CAN_TEST_LOOPBACK
        bool sent = can_test_send(0x320, probe, sizeof(probe),
                                  pdMS_TO_TICKS(100));
        bool received = sent && can_test_receive(&rx, pdMS_TO_TICKS(100));

        if (received && rx.StdId == 0x320U &&
            rx.DLC == sizeof(probe) &&
            memcmp(rx.Data, probe, sizeof(probe)) == 0)
            printf("CAN loopback OK\r\n");
        else
            printf("CAN loopback FAIL\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
#else
        uint8_t heartbeat[1] = {seq++};

        // STM32 每两秒发 0x320；同时轮询 i.MX6ULL
        if (!can_test_send(0x320, heartbeat, sizeof(heartbeat),
                           pdMS_TO_TICKS(100)))
            printf("CAN 0x320 TX failed\r\n");

        for (uint16_t i = 0; i < 100U; ++i)
        {
            if (can_test_receive(&rx, pdMS_TO_TICKS(20)))
            {
                printf("CAN 0x%x RX, DLC=%u, Msg = ", rx.StdId, rx.DLC);
                for (uint8_t i = 0; i < rx.DLC; i++)
                    printf("%02x ", rx.Data[i]);
                printf("\r\n");
                // 收到数据后，以 0x322 原样回复其数据
                if (!can_test_send(0x322, rx.Data, rx.DLC, pdMS_TO_TICKS(100)))
                    printf("CAN 0x322 TX failed\r\n");
            }
        }
#endif
    }
}
