#include <stdint.h>
#include <string.h>
#include "TFT_LCD.h"
#include "FreeRTOS.h"
#include "task.h"
#include "font.h"
#include "page.h"
#include "app.h"
#include "app_ui.h"

#if (ENABLE_LVGL_USE == 1)
static lv_obj_t *wifi_scr;
static lv_obj_t *wifi_load_bar;
#endif

void wifi_page_display(void)
{
#if (ENABLE_LVGL_USE == 0)
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
#endif
#if (ENABLE_LVGL_USE == 1)
    wifi_scr = lv_obj_create(NULL);
    lv_scr_load(wifi_scr); // 加载这个屏幕

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    uint8_t start_y = 10;
    /*  =======================图片=======================  */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_wifi);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, start_y);

    //================================文字1================================
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_obj_set_width(label, 240); // 标签的高度和宽度
    lv_obj_set_height(label, 32);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, start_y + img_wifi.header.h + 10);
    lv_label_set_text(label, "WiFi");
    // 设置字体大小和位置
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体颜色
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    // 设置背景颜色
    lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(label, 0, 0); // 设置背景透明度
    // ================================文字2================================
    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_obj_set_width(label2, 240); // 标签的高度和宽度
    lv_obj_set_height(label2, 30);
    lv_obj_align(label2, LV_ALIGN_TOP_MID, 0, start_y + img_wifi.header.h + 10 * 2 + lv_font_montserrat_32.line_height);
    lv_label_set_text(label2, WIFI_SSID);
    // 设置字体大小和位置
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体颜色
    lv_obj_set_style_text_color(label2, lv_color_hex(0xffffff), 0);
    // 设置背景颜色
    lv_obj_set_style_bg_color(label2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(label2, 0, 0); // 设置背景透明度

    // ================================文字3================================
    lv_obj_t *label3 = lv_label_create(lv_scr_act());
    lv_obj_set_width(label3, 240); // 标签的高度和宽度
    lv_obj_set_height(label3, 30);
    lv_obj_align(label3, LV_ALIGN_TOP_MID, 0, start_y + img_wifi.header.h + 10 * 3 + lv_font_montserrat_32.line_height + lv_font_montserrat_24.line_height);
    lv_label_set_text(label3, "Connecting...");
    // 设置字体大小和位置
    lv_obj_set_style_text_font(label3, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(label3, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体颜色
    lv_obj_set_style_text_color(label3, lv_color_hex(0xffffff), 0);
    // 设置背景颜色
    lv_obj_set_style_bg_color(label3, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(label3, 0, 0); // 设置背景透明度

    //  ================================进度条控件================================

    // // 创建进度条控件
    // wifi_load_bar = lv_bar_create(wifi_scr);
    // lv_obj_set_size(wifi_load_bar, 200, 20);                 // 设置宽高
    // lv_obj_align(wifi_load_bar, LV_ALIGN_BOTTOM_MID, 0, 10); // 居中显示

    // //
    // lv_bar_set_range(wifi_load_bar, 0, 100);
    // lv_bar_set_value(wifi_load_bar, 0, LV_ANIM_OFF);

#endif
}
