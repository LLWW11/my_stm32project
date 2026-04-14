#include "TFT_LCD.h"
#include "TFT_Img.h"
#include "string.h"

void error_page_display(const char *msg)
{
    TFT_SetWindow(0, 0, 240, 360, BLACK);
    TFT_LCD_show_img(40, 20, &img_error);
    // 160*160 240*320
    int len = strlen(msg) * font32_youyuan.height / 2;
    uint8_t startx = 0;
    if (len < TFT_COLUMN_NUMBER)
        startx = (TFT_COLUMN_NUMBER - len) / 2;

    TFT_LCD_Write_String(startx,
                        254,
                        msg,
                        &font32_youyuan,
                        BLACK,
                        YELLOW);
}