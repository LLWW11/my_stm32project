#include "TFT_LCD.h"
#include "TFT_Img.h"
#include "app_ui.h"
#include "lvgl.h"
#include "font.h"

#if (ENABLE_LVGL_USE == 1)
static lv_obj_t *wifi_scr;
static lv_obj_t *wifi_load_bar;
#endif
void welcome_page_display(void)
{
#if (ENABLE_LVGL_USE == 0)
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
                    WHITE);
#elif (ENABLE_LVGL_USE == 1)
    // 设置当前屏幕的背景色为黑色
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    /*  图片  */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_meihua);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 20);

    /*  文字  */
    // 创建标签
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_obj_set_width(label, 240); // 标签的高度和宽度
    lv_obj_set_height(label, 25);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 240);

    lv_label_set_text(label, "Hello World!");
    // 设置字体大小和位置
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体颜色
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    // 设置背景颜色
    lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(label, 0, 0); // 设置背景透明度

    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_obj_set_width(label2, 240); // 标签的高度和宽度
    lv_obj_set_height(label2, 25);
    lv_obj_align(label2, LV_ALIGN_TOP_MID, 0, 284);

    lv_label_set_text(label2, "Waiting...");
    // 设置字体大小和位置
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体颜色
    lv_obj_set_style_text_color(label2, lv_color_hex(0xffffff), 0);
    // 设置背景颜色
    lv_obj_set_style_bg_color(label2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(label2, 0, 0); // 设置背景透明度

    // 创建进度条控件
    wifi_load_bar = lv_bar_create(wifi_scr);
    lv_obj_set_size(wifi_load_bar, 240, 10);                // 设置宽高
    lv_obj_align(wifi_load_bar, LV_ALIGN_BOTTOM_MID, 0, 0); // 居中显示
    lv_bar_set_range(wifi_load_bar, 0, 100);
    lv_bar_set_value(wifi_load_bar, 0, LV_ANIM_OFF);
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}
#if (ENABLE_LVGL_USE == 1)
void wifi_page_update_bar(uint8_t percentage)
{
    if (wifi_load_bar != NULL)
    {
        lv_bar_set_value(wifi_load_bar, percentage, LV_ANIM_ON);
    }
}
#endif
