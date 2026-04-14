#include <stdio.h>
#include <stdint.h>
#include "esp_at.h"
#include "app.h"
#include "page.h"
#include "delay.h"
#include <string.h>

#define SSID "vivo50"
#define pwd "17754089085LW"

void wifi_init(void)
{
    if (!esp_at_WiFi_Init())
    {
        printf("[WIFI] init failed!\r\n");
        goto err;
    }
    printf("[WIFI] init successfully\n");
    if (!esp_at_sntp_Init())
    {
        printf("[SNTP] init failed\r\n");
        goto err;
    }
    printf("[SNTP] init successfully\n");
    return;
err:
    error_page_display("[WiFi] failed");
    while (1)
    {
        // wifi_init();
    }
}

void wait_wifi_connet(void)
{
    wifi_page_display();
    delay_ms(1000);
    if (esp_connetWiFi(SSID, pwd, NULL))
    {
        printf("[WIFI] connect successful\r\n");
        return;
    }
    for (uint32_t i = 0; i < 10 * 1000; i += 100)
    {
        esp_wifi_info_t wifi;
        memset(&wifi, 0, sizeof(wifi));
        delay_ms(100);
        if (esp_get_wifi_info(&wifi) && wifi.isConnect)
        {
            printf("[WiFi] Connected\r\n");
            printf("[WiFi] SSID:%s, Mac:%s\r\n", wifi.ssid, wifi.mac);
            return;
        }
    }

    printf("[WIFI] Connection Timeout\r\n");
    error_page_display("WiFi Connect Failed");
    while (1)
    {
        ;
    }
}