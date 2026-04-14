#include <stdio.h>
#include <stdint.h>
#include "esp_at.h"
#include "app.h"
#include "app_ui.h"
#include "page.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "dbg_config.h"
// #define SSID "vivo50"
// #define PWD  "17754089085LW"

extern void wifi_page_display(void);
extern void wifi_page_update_bar(uint8_t percentage);

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
#if (ENABLE_LVGL_USE == 1)
    if (xGuiMutex != NULL && xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        wifi_page_display();
        xSemaphoreGive(xGuiMutex);
    }
#elif (ENABLE_LVGL_USE == 0)
    wifi_page_display();
#endif
    esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWD, NULL);

    for (uint32_t t = 0; t < 10 * 1000; t += 100)
    {
        vTaskDelay(pdMS_TO_TICKS(100));

#if ENABLE_LVGL_USE
        uint8_t progress = (t * 100) / (10 * 1000);
        if (xGuiMutex != NULL && xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
        {
            wifi_page_update_bar(progress); // 安全更新进度条
            xSemaphoreGive(xGuiMutex);
        }
#endif
        esp_wifi_info_t wifi = {0};
        if (esp_at_get_wifi_info(&wifi) && wifi.isConnect)
        {
#if ENABLE_LVGL_USE
            if (xGuiMutex != NULL && xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
            {
                wifi_page_update_bar(100); // 连上后将进度条拉满
                xSemaphoreGive(xGuiMutex);
            }
            // 稍作延时，让用户能看清进度条满了，并且看到连接成功的状态
            vTaskDelay(pdMS_TO_TICKS(800));
#endif
            // wifi_page_display();
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
        vTaskDelay(pdMS_TO_TICKS(1000));
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