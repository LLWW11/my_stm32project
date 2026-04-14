#include <stdio.h>
#include <stdint.h>
#include "esp_at.h"
#include "app.h"
#include "page.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
// #define SSID "vivo50"
// #define PWD  "17754089085LW"

void wifi_init(void)
{
    if (!esp_at_WiFi_Init())
    {
#if ENABLE_DEBUG_PRINT
        printf("[WIFI] init failed!\r\n");
#endif
        goto err;
    }
#if ENABLE_DEBUG_PRINT
    printf("[WIFI] init successfully\n");
#endif
    if (!esp_at_sntp_Init())
    {
#if ENABLE_DEBUG_PRINT
        printf("[SNTP] init failed\r\n");
#endif
        goto err;
    }
#if ENABLE_DEBUG_PRINT
    printf("[SNTP] init successfully\n");
#endif
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
#if ENABLE_DEBUG_PRINT
    printf("[WIFI] connecting\n");
#endif
    esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWD, NULL);

    for (uint32_t t = 0; t < 10 * 1000; t += 100)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_info_t wifi = {0};
        if (esp_at_get_wifi_info(&wifi) && wifi.isConnect)
        {
            wifi_page_display();
#if ENABLE_DEBUG_PRINT
            printf("[WIFI] Connected\n");
            printf("[WIFI] SSID: %s, Mac: %s, RSSI: %d\n",
                   wifi.ssid, wifi.pwd, wifi.rssi);
#endif
            return;
        }
    }
#if ENABLE_DEBUG_PRINT
    printf("[WIFI] Connection Timeout\n");
#endif
    error_page_display("wireless connect failed");
    while (1)
    {
        ;
    }
}
/*
void wait_wifi_connet(void)
{
    wifi_page_display();
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (esp_connetWiFi(SSID, pwd, NULL))
    {
        printf("[WIFI] connect successful\r\n");
        return;
    }
    for (uint32_t i = 0; i < 10 * 1000; i += 100)
    {
        esp_wifi_info_t wifi;
        memset(&wifi, 0, sizeof(wifi));
        vTaskDelay(pdMS_TO_TICKS(100));
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
*/