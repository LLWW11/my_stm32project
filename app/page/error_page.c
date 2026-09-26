#include "TFT_LCD.h"
#include "TFT_Img.h"
#include "string.h"
#include "app_ui.h"
#include "dbg_config.h"

#if (ENABLE_LVGL_USE == 1)
static lv_obj_t *err_src;
#endif
void error_page_display(const char *msg)
{

// 160*160 240*320
#if (ENABLE_LVGL_USE == 0)
    int len = strlen(msg) * font32_youyuan.height / 2;
    uint8_t startx = 0;
    if (len < TFT_COLUMN_NUMBER)
        startx = (TFT_COLUMN_NUMBER - len) / 2;
    ui_set_window(0, 0, 240, 360, BLACK);
    ui_draw_image(40, 20, &img_error);
    ui_write_string(startx,
                    254,
                    msg,
                    &font32_youyuan,
                    BLACK,
                    YELLOW);
#elif (ENABLE_LVGL_USE == 1)
    err_src = lv_obj_create(NULL);
    lv_scr_load(err_src); // 加载这个屏幕

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    uint8_t start_y = 10;
    /*  =======================图片=======================  */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_error);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, start_y);
#endif
}