#include <stdint.h>
#include <string.h>
#include "TFT_LCD.h"
#include "font.h"
#include "page.h"
#include "app.h"

void wifi_page_display(void)
{
    static const char *ssid = WIFI_SSID;
    uint16_t ssid_startx = 0;
    int ssid_len = strlen(ssid) * font32_youyuan.height / 2;
    if (ssid_len < TFT_COLUMN_NUMBER)
        ssid_startx = (TFT_COLUMN_NUMBER - ssid_len + 1) / 2;

    TFT_SetWindow(0, 0, TFT_COLUMN_NUMBER, TFT_LINE_NUMBER, BLACK);
    TFT_LCD_show_img(30, 15, &img_wifi);
    TFT_LCD_Write_String(88, 191, "WiFi", &font32_youyuan, BLACK, BLUE);
    TFT_LCD_Write_String(ssid_startx, 231, ssid, &font32_youyuan, BLACK, PINK);
    TFT_LCD_Write_String(55, 273, "Connecting...", &font24_maple_bold, BLACK, CYAN);
    //240-16*4 = 
}
