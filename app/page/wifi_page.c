#include <stdint.h>
#include <string.h>
#include "TFT_LCD.h"
#include "FreeRTOS.h"
#include "task.h"
#include "font.h"
#include "page.h"
#include "app.h"
#include "app_ui.h"

void wifi_page_display(void)
{
    static const char *ssid = WIFI_SSID;
    uint16_t ssid_startx = 0;
    int ssid_len = strlen(ssid) * font32_youyuan.height / 2;
    if (ssid_len < TFT_COLUMN_NUMBER)
        ssid_startx = (TFT_COLUMN_NUMBER - ssid_len + 1) / 2;

    ui_set_window(0, 0, TFT_COLUMN_NUMBER, TFT_LINE_NUMBER, BLACK);
    ui_draw_image(30, 15, &img_wifi);
    ui_write_string(88, 191, "WiFi", &font32_youyuan, BLACK, BLUE);
    ui_write_string(ssid_startx, 231, ssid, &font32_youyuan, BLACK, PINK);
    ui_write_string(55, 273, "Connecting...", &font24_maple_bold, BLACK, CYAN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    //240-16*4 = 
}
