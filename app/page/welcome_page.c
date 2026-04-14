#include "TFT_LCD.h"
#include "TFT_Img.h"

void welcome_page_display(void)
{
    TFT_SetWindow(0, 0, 240, 320, BLACK);
    TFT_LCD_show_img(0, 0, &img_laoba);
    TFT_LCD_Write_String(40,
                        250,
                        "老八嵌入式",
                        &font32_youyuan,
                        BLACK,
                        WHITE);
    TFT_LCD_Write_String(30,
                        288,
                        "奥利给,干了!",
                        &font32_youyuan,
                        BLACK,
                        WHITE); // 240*320  240+32=272
}