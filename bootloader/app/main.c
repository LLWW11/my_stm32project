#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdlib.h>
#include "boot_UART.h"

#define BOOT_APP_BASE_ADDRESS   0x08010000U// WeatherClock APP在内部Flash的起始地址
#define BOOT_APP_END_ADDRESS    0x08100000U// APP 分区结束地址，不包含该地址本身
#define BOOT_SRAM_START_ADDRESS 0x20000000U //主SRAM起始地址
#define BOOT_SRAM_END_ADDRESS   0x20020000U //初始MSP

//   APP 复位入口函数指针类型
typedef void (*boot_app_entry_t)(void);

//  轮询延时
static void boot_delay_ms(uint32_t milliseconds)
{
    uint32_t reload_value;

    reload_value = SystemCoreClock / 1000U;

    if ((reload_value == 0U) ||
        ((reload_value - 1U) > SysTick_LOAD_RELOAD_Msk))
    {
        return;
    }

    SysTick->LOAD = reload_value - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_ENABLE_Msk;

    while (milliseconds > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U);

        milliseconds--;
    }

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
}

//APP 向量表合法时返回 true
static bool boot_app_is_valid(uint32_t *app_msp,
                              uint32_t *app_reset_handler)
{
    uint32_t reset_code_address;

    if ((app_msp == NULL) || (app_reset_handler == NULL))
        return false;

    *app_msp = *(volatile uint32_t *)BOOT_APP_BASE_ADDRESS;
    *app_reset_handler = *(volatile uint32_t *)(BOOT_APP_BASE_ADDRESS + 4U);

    // 是否烧录APP 
    if ((*app_msp == 0xFFFFFFFFU) ||(*app_reset_handler == 0xFFFFFFFFU))
        return false;
    // MSP是否合法
    if ((*app_msp < BOOT_SRAM_START_ADDRESS) ||
        (*app_msp > BOOT_SRAM_END_ADDRESS))
        return false;
    // Cortex-M 的栈要求至少按照 8 字节对齐
    if ((*app_msp & 0x7U) != 0U)
        return false;
    // Reset_Handler 最低位必须为 1，表示 Thumb 状态
    if ((*app_reset_handler & 0x1U) == 0U)
        return false;
    // 真正的代码地址是否在APP FLASH区间
    reset_code_address = *app_reset_handler & ~0x1U;
    if ((reset_code_address < BOOT_APP_BASE_ADDRESS) || 
        (reset_code_address >= BOOT_APP_END_ADDRESS))
        return false;

    return true;
}



// 切换到 APP 的主栈并跳转到 APP 复位入口

__asm void boot_start_app(uint32_t app_msp,
                          uint32_t app_reset_handler)
{
    MSR MSP, R0
    CPSIE I
    BX R1
}

//清理 Bootloader运行状态并跳转到 APP
static void boot_jump_to_app(uint32_t app_msp,
                             uint32_t app_reset_handler)
{
    uint32_t index;

    USART_Cmd(USART1, DISABLE); //关闭UART1
    __disable_irq(); //关闭中断
    // 关闭并复位 SysTick 和 PendSV
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk |
                SCB_ICSR_PENDSVCLR_Msk;
    //清除所有外部中断
    for (index = 0U; index < 8U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFU;
        NVIC->ICPR[index] = 0xFFFFFFFFU;
    }

    // 把中断向量表切换到 APP
    SCB->VTOR = BOOT_APP_BASE_ADDRESS;
    __DSB();  // 数据同步屏障，确保前面的寄存器写入全部完成
    __ISB();  // 指令同步屏障，清空 CPU 指令流水线，确保后续执行使用新的配置

    __set_CONTROL(0U);  //使用 MSP
    __ISB();

    //跳转
    boot_start_app(app_msp, app_reset_handler);

    while (1)
    {
        //正常情况下不会跑到到这里
    }
}


//Bootloader 程序入口。
int main(void)
{
    uint32_t app_msp;
    uint32_t app_reset_handler;

    boot_uart1_init();

    boot_uart1_send_string("\r\n[BOOT] Bootloader start\r\n");

    if (boot_app_is_valid(&app_msp, &app_reset_handler))
    {
        boot_uart1_send_string("[BOOT] APP MSP  = ");
        boot_uart1_send_hex32(app_msp);
        boot_uart1_send_string("\r\n");

        boot_uart1_send_string("[BOOT] APP Reset = ");
        boot_uart1_send_hex32(app_reset_handler);
        boot_uart1_send_string("\r\n");

        boot_uart1_send_string("[BOOT] APP valid\r\n");
 
        boot_uart1_send_string("[BOOT] Jump in 5...\r\n");
        boot_delay_ms(1000U);        
        boot_uart1_send_string("[BOOT] Jump in 4...\r\n");
        boot_delay_ms(1000U);
        boot_uart1_send_string("[BOOT] Jump in 3...\r\n");
        boot_delay_ms(1000U);

        boot_uart1_send_string("[BOOT] Jump in 2...\r\n");
        boot_delay_ms(1000U);

        boot_uart1_send_string("[BOOT] Jump in 1...\r\n");
        boot_delay_ms(1000U);

        boot_uart1_send_string("[BOOT] Jump now\r\n");

        boot_jump_to_app(app_msp, app_reset_handler);
    }
    // APP 无效时留在 Bootloader
    boot_uart1_send_string("[BOOT] APP invalid\r\n");
    boot_uart1_send_string("[BOOT] Stay in bootloader\r\n");

    while (1)
    {
        //正常情况下不会跑到到这里
    }
}
