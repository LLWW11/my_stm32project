#include "TFT_LCD.h"
#include "TFT_Img.h"
#include "app_ui.h"

void welcome_page_display(void)
{
    ui_set_window(0, 0, 240, 320, BLACK);
    ui_draw_image(0, 0, &img_laoba);
    ui_write_string(40,
                    250,
                    "老八嵌入式",
                    &font32_youyuan,
                    BLACK,
                    WHITE);
    ui_write_string(30,
                    288,
                    "奥利给,干了!",
                    &font32_youyuan,
                    BLACK,
                    WHITE); // 240*320  240+32=272
}