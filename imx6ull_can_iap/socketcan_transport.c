#define _POSIX_C_SOURCE 200809L
#include "can_iap_sender.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int fd;
} socketcan_context_t;

/** 返回单调毫秒，避免系统校时影响传输超时。 */
static uint64_t monotonic_ms(void *context)
{
    struct timespec now;
    (void)context;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
}

/** 有界重试本地发送缓冲拥塞，成功只代表内核接收，落盘仍需协议 ACK。 */
static int socketcan_send(void *context, const iap_frame_t *frame)
{
    socketcan_context_t *can = context;
    struct can_frame wire;
    uint64_t deadline = monotonic_ms(NULL) + 250U;
    struct timespec pause = {0, 5000000};
    if (!frame || frame->id > CAN_SFF_MASK || frame->len > 8) return -1;
    memset(&wire, 0, sizeof(wire));
    wire.can_id = frame->id;
    wire.can_dlc = frame->len; /* 兼容 i.MX6ULL 较旧的 Linux CAN 头文件。 */
    memcpy(wire.data, frame->data, frame->len);
    for (;;) {
        ssize_t size = write(can->fd, &wire, sizeof(wire));
        if (size == (ssize_t)sizeof(wire)) return 0;
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                         errno == ENOBUFS || errno == EINTR) &&
            monotonic_ms(NULL) < deadline) {
            /* POLLOUT 在 ENOBUFS 时可能持续有效，短暂休眠防止忙循环。 */
            nanosleep(&pause, NULL);
            continue;
        }
        if (size < 0) perror("SocketCAN write");
        else fprintf(stderr, "SocketCAN short write\n");
        return -1;
    }
}

/** 等待一帧有效标准回复，遇到 bus-off 或接口故障立即返回失败。 */
static int socketcan_receive(void *context, iap_frame_t *frame, unsigned timeout_ms)
{
    socketcan_context_t *can = context;
    uint64_t deadline = monotonic_ms(NULL) + timeout_ms;
    for (;;) {
        struct pollfd item = {can->fd, POLLIN, 0};
        struct can_frame wire;
        uint64_t now = monotonic_ms(NULL);
        int wait_ms = now < deadline ? (int)(deadline - now) : 0;
        int result = poll(&item, 1, wait_ms);
        ssize_t size;
        if (result == 0) return 0;
        if (result < 0) {
            if (errno == EINTR && monotonic_ms(NULL) < deadline) continue;
            perror("SocketCAN poll");
            return -1;
        }
        if (item.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "SocketCAN interface/socket error: 0x%x\n", item.revents);
            return -1;
        }
        size = read(can->fd, &wire, sizeof(wire));
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (monotonic_ms(NULL) >= deadline) return 0;
            continue;
        }
        if (size != (ssize_t)sizeof(wire)) {
            if (size < 0) perror("SocketCAN read");
            else fprintf(stderr, "SocketCAN short frame\n");
            return -1;
        }
        if (wire.can_id & CAN_ERR_FLAG) {
            fprintf(stderr, "CAN error frame: 0x%08lX; check ip -details -statistics\n",
                    (unsigned long)wire.can_id);
            return -1;
        }
        if (!(wire.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) &&
            wire.can_id == IAP_REPLY_ID && wire.can_dlc <= 8) {
            frame->id = (uint16_t)wire.can_id;
            frame->len = wire.can_dlc;
            memcpy(frame->data, wire.data, 8);
            return 1;
        }
        if (monotonic_ms(NULL) >= deadline) return 0;
    }
}

/** 建立只接收 0x123 标准数据回复的 SocketCAN 通道。 */
int iap_socketcan_open(iap_link_t *link, const char *interface_name)
{
    socketcan_context_t *can;
    struct sockaddr_can address;
    struct can_filter filter;
    can_err_mask_t error_mask = CAN_ERR_BUSOFF | CAN_ERR_TX_TIMEOUT;
    unsigned index;
    int flags;
    int own_messages = 0;
    if (!link || !interface_name) return -1;
    memset(link, 0, sizeof(*link));
    index = if_nametoindex(interface_name);
    if (!index) { perror("CAN interface"); return -1; }
    can = malloc(sizeof(*can));
    if (!can) { perror("malloc"); return -1; }
    can->fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can->fd < 0) { perror("CAN_RAW socket"); free(can); return -1; }
    flags = fcntl(can->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(can->fd, F_SETFL, flags | O_NONBLOCK) < 0) goto fail;
    filter.can_id = IAP_REPLY_ID;
    filter.can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG;
    if (setsockopt(can->fd, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter)) < 0 ||
        setsockopt(can->fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
                   &error_mask, sizeof(error_mask)) < 0 ||
        setsockopt(can->fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                   &own_messages, sizeof(own_messages)) < 0) goto fail;
    memset(&address, 0, sizeof(address));
    address.can_family = AF_CAN;
    address.can_ifindex = (int)index;
    if (bind(can->fd, (struct sockaddr *)&address, sizeof(address)) < 0) goto fail;
    link->context = can;
    link->send = socketcan_send;
    link->receive = socketcan_receive;
    link->now_ms = monotonic_ms;
    return 0;
fail:
    perror("SocketCAN setup");
    close(can->fd);
    free(can);
    return -1;
}

/** 释放传输资源；不会发送 END，也不会自动重启接收会话。 */
void iap_socketcan_close(iap_link_t *link)
{
    if (link && link->context) {
        socketcan_context_t *can = link->context;
        close(can->fd);
        free(can);
        memset(link, 0, sizeof(*link));
    }
}
