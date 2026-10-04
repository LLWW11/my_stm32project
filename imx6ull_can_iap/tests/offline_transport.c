#include "can_iap_sender.h"

/* 仅用于主机命令行预检。正式程序必须链接 socketcan_transport.c。 */
/** 离线测试拒绝打开 CAN，保证参数预检不会接触硬件。 */
int iap_socketcan_open(iap_link_t *link, const char *interface_name)
{
    (void)link;
    (void)interface_name;
    fprintf(stderr, "Offline test binary: use --dry-run\n");
    return -1;
}
/** 离线测试没有套接字资源。 */
void iap_socketcan_close(iap_link_t *link) { (void)link; }
