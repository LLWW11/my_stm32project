#include "TFT_Img.h"
#include "TFT_LCD.h"
#include "app.h"
#include "esp_at.h"
#include "rtc.h"
#include "app_ui.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dbg_config.h"
#include "app_ui.h"
#include "page.h"
#if ENABLE_LVGL_USE
static lv_obj_t *main_scr;

static lv_obj_t *label_hour;
static lv_obj_t *label_colon;
static lv_obj_t *label_min;
static lv_obj_t *label_city;
static lv_obj_t *label_wifi_id;

static lv_obj_t *label_date;           // 日期标签
static lv_obj_t *label_inner_temp;     // 室内温度标签
static lv_obj_t *label_outdoor_temp;   // 室外温度标签
static lv_obj_t *label_inner_humidity; // 室内温度标签
// static lv_obj_t *img_weather_icon;     // 天气图标
static lv_obj_t *img_wifi_icon;        // 图标
LV_FONT_DECLARE(lv_font_montserrat_80_partial);
LV_FONT_DECLARE(my_font_24);
LV_FONT_DECLARE(lv_font_montserrat_52_partial);

static void subpage_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *subpage = lv_event_get_target(e);

    if (code == LV_EVENT_KEY)
    {
        lv_group_t *g_main = (lv_group_t *)lv_event_get_user_data(e);
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC)
        {
            lv_group_t *g_sub = lv_obj_get_group(subpage);
            if (g_sub)
                lv_group_del(g_sub);
            lv_obj_del(subpage);
            extern lv_indev_t *keypad_indev;
            if (keypad_indev)
            {
                lv_indev_set_group(keypad_indev, g_main);
            }
        }
    }
}

static void square_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        const char *text = (const char *)lv_event_get_user_data(e);
        lv_obj_t *target_square = lv_event_get_target(e); // 获取当前被点击的对象
        lv_group_t *g_main = lv_obj_get_group(lv_event_get_target(e));

        lv_color_t square_color = lv_obj_get_style_bg_color(target_square, LV_PART_MAIN);
        lv_obj_t *subpage = lv_obj_create(lv_scr_act());
        lv_obj_set_size(subpage, 240, 320);
        lv_obj_center(subpage);
        lv_obj_set_style_bg_color(subpage, square_color, 0);
        lv_obj_set_style_border_width(subpage, 0, 0);

        void (*display_func)(lv_obj_t *) = (void (*)(lv_obj_t *))lv_event_get_user_data(e);
        if (display_func)
        {
            display_func(subpage);
        }

        lv_group_t *g_sub = lv_group_create();
        lv_group_add_obj(g_sub, subpage);

        lv_obj_add_event_cb(subpage, subpage_event_cb, LV_EVENT_KEY, g_main);

        extern lv_indev_t *keypad_indev;
        if (keypad_indev)
        {
            lv_indev_set_group(keypad_indev, g_sub);
        }
    }
}

static void main_time_update_cb(lv_timer_t *t)
{
    static rtc_time_t last_time = {0};
    rtc_time_t current_time;
    rtc_get_time(&current_time);

    if (current_time.year < 2020)
    {
        bool blink = (current_time.second % 2 == 0);
        lv_label_set_text(label_colon, blink ? ":" : "");
        return;
    }

    if (memcmp(&current_time, &last_time, sizeof(rtc_time_t)) == 0)
        return;

    bool is2sec = (current_time.second % 2 == 0);
    bool refresh = (current_time.minute != last_time.minute) || (last_time.year == 0);

    memcpy(&last_time, &current_time, sizeof(rtc_time_t));

    lv_label_set_text(label_colon, is2sec ? ":" : "");

    if (refresh)
    {
        char hour_str[3];
        char min_str[3];
        snprintf(hour_str, sizeof(hour_str), "%02d", current_time.hour);
        snprintf(min_str, sizeof(min_str), "%02d", current_time.minute);
        lv_label_set_text(label_hour, hour_str);
        lv_label_set_text(label_min, min_str);

        char weekday[15];
        char str[25];
        switch (current_time.weekday)
        {
        case 1:
            strcpy(weekday, "星期一");
            break;
        case 2:
            strcpy(weekday, "星期二");
            break;
        case 3:
            strcpy(weekday, "星期三");
            break;
        case 4:
            strcpy(weekday, "星期四");
            break;
        case 5:
            strcpy(weekday, "星期五");
            break;
        case 6:
            strcpy(weekday, "星期六");
            break;
        case 7:
            strcpy(weekday, "星期日");
            break;
        default:
            strcpy(weekday, "X");
            break;
        }
        snprintf(str, sizeof(str), "%04d/%02d/%02d %s", current_time.year, current_time.month, current_time.date, weekday);
        lv_label_set_text(label_date, str);
    }
}

#endif
void main_page_display(void)
{
#if (ENABLE_LVGL_USE == 1)
    main_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_scr, lv_color_hex(0x000000), 0); // 0x00ffff 0xffffff
    lv_scr_load(main_scr);

    extern lv_indev_t *keypad_indev;
    lv_group_t *g = lv_group_create();
    if (keypad_indev)
        lv_indev_set_group(keypad_indev, g);

    static lv_style_t style_focus;
    lv_style_init(&style_focus);
    lv_style_set_border_color(&style_focus, lv_color_hex(0xFF0000));
    lv_style_set_border_width(&style_focus, 3);

    // ==========================上部方块=================================
    lv_obj_t *square0 = lv_obj_create(main_scr);
    lv_obj_set_size(square0, 240, 156);
    lv_obj_set_pos(square0, 0, 0);
    lv_obj_set_style_bg_color(square0, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(square0, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(square0, 15, LV_PART_MAIN);
    lv_obj_clear_flag(square0, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(square0, &style_focus, LV_STATE_FOCUSED);
    lv_group_add_obj(g, square0);
    lv_obj_add_event_cb(square0, square_event_cb, LV_EVENT_CLICKED, page_square0_display);
    lv_obj_move_background(square0);
    // 左上角显示wifi图标
    img_wifi_icon = lv_img_create(main_scr);
    lv_obj_move_foreground(img_wifi_icon); // 将此对象移到最上层
    lv_img_set_src(img_wifi_icon, &icon_wifi);
    lv_obj_align(img_wifi_icon, LV_ALIGN_TOP_LEFT, 0, 0);
    // 右上角显示wifi名称
    label_wifi_id = lv_label_create(main_scr);
    lv_obj_move_foreground(label_wifi_id); // 将此对象移到最上层
    // lv_obj_set_width(label_wifi_id, 190);
    lv_obj_set_height(label_wifi_id, 25);
    lv_obj_align(label_wifi_id, LV_ALIGN_TOP_RIGHT, 0, 0);
    // lv_label_set_long_mode(label_wifi_id, LV_LABEL_LONG_DOT);
    lv_label_set_text(label_wifi_id, "[----]");
    lv_obj_set_style_text_font(label_wifi_id, &lv_font_montserrat_18, 0); // 设置字体大小和位置
    lv_obj_set_style_text_align(label_wifi_id, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(label_wifi_id, lv_color_hex(0x000000), 0); // 设置字体颜色
    lv_obj_set_style_bg_color(label_wifi_id, lv_color_hex(0xffffff), 0);   // 设置背景颜色
    lv_obj_set_style_bg_opa(label_wifi_id, 0, 0);                          // 设置背景透明度

    // Start an LVGL timer to update time without blocking on HTTP
    lv_timer_create(main_time_update_cb, 1000, NULL);

    // 创建中间的冒号
    label_colon = lv_label_create(main_scr);
    lv_obj_set_width(label_colon, 20);
    lv_obj_set_height(label_colon, 120);
    lv_obj_set_style_text_font(label_colon, &lv_font_montserrat_80_partial, 0);
    lv_label_set_text(label_colon, ":");
    lv_obj_align(label_colon, LV_ALIGN_TOP_MID, 0, 35);
    // 小时，直接固定死文本框位置和文字位置
    label_hour = lv_label_create(main_scr);
    lv_obj_set_width(label_hour, 110);
    lv_obj_set_height(label_hour, 120);
    lv_obj_set_style_text_align(label_hour, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_hour, &lv_font_montserrat_80_partial, 0);
    lv_label_set_text(label_hour, "--");
    lv_obj_set_pos(label_hour, 0, 35);
    lv_obj_set_style_text_align(label_hour, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    // 分钟，直接固定死文本框位置和文字位置
    label_min = lv_label_create(main_scr);
    lv_obj_set_width(label_min, 110);
    lv_obj_set_height(label_min, 120);
    lv_obj_set_style_text_font(label_min, &lv_font_montserrat_80_partial, 0);
    lv_label_set_text(label_min, "--");
    lv_obj_set_pos(label_min, 110 + 20, 35);
    lv_obj_set_style_text_align(label_min, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
    // 日期
    label_date = lv_label_create(main_scr);
    lv_obj_set_width(label_date, 240);
    lv_obj_set_height(label_date, 30);
    lv_obj_set_style_text_font(label_date, &my_font_24, 0);
    lv_label_set_text(label_date, "----/--/- 星期日");
    lv_obj_set_pos(label_date, 0, 115);
    lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    //==============================左下角方块=======================================
    lv_obj_t *square1 = lv_obj_create(main_scr);
    lv_obj_clear_flag(square1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(square1, 116, 156);
    lv_obj_set_style_bg_color(square1, lv_color_hex(0x00ffff), 0);
    lv_obj_align(square1, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_radius(square1, 15, LV_PART_MAIN);
    lv_obj_clear_flag(square1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(square1, &style_focus, LV_STATE_FOCUSED);
    lv_group_add_obj(g, square1);
    lv_obj_add_event_cb(square1, square_event_cb, LV_EVENT_CLICKED, page_square1_display);
    //"室内环境"四个大字
    lv_obj_t *label_shinei = lv_label_create(square1);
    lv_obj_set_width(label_shinei, 114);
    lv_obj_set_height(label_shinei, 30);
    lv_obj_set_style_text_font(label_shinei, &my_font_24, 0);
    lv_label_set_text(label_shinei, "室内环境");
    lv_obj_set_pos(label_shinei, -10, -10);
    lv_obj_set_style_text_align(label_shinei, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    // 定死的摄氏度和百分比单位
    lv_obj_t *label_sheshidu = lv_label_create(square1);
    lv_obj_set_width(label_sheshidu, 112);
    lv_obj_set_height(label_sheshidu, 55);
    lv_obj_set_style_text_font(label_sheshidu, &my_font_24, 0);
    lv_label_set_text(label_sheshidu, "℃");
    lv_obj_set_pos(label_sheshidu, -10, -10 + 40);
    lv_obj_set_style_text_align(label_sheshidu, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_obj_t *label_percent = lv_label_create(square1);
    lv_obj_set_width(label_percent, 112);
    lv_obj_set_height(label_percent, 55);
    lv_obj_set_style_text_font(label_percent, &lv_font_montserrat_24, 0);
    lv_label_set_text(label_percent, "%");
    lv_obj_set_pos(label_percent, -13, -13 + 90);
    lv_obj_set_style_text_align(label_percent, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);

    label_inner_temp = lv_label_create(square1);
    lv_obj_move_foreground(label_inner_temp);
    lv_obj_set_width(label_inner_temp, 100);
    lv_obj_set_height(label_inner_temp, 55);
    lv_obj_set_style_text_font(label_inner_temp, &lv_font_montserrat_52_partial, 0);
    lv_label_set_text(label_inner_temp, "---");
    lv_obj_set_pos(label_inner_temp, -10, -10 + 40);
    lv_obj_set_style_text_align(label_inner_temp, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);

    label_inner_humidity = lv_label_create(square1);
    lv_obj_move_foreground(label_inner_humidity);
    lv_obj_set_width(label_inner_humidity, 100);
    lv_obj_set_height(label_inner_humidity, 55);
    lv_obj_set_style_text_font(label_inner_humidity, &lv_font_montserrat_52_partial, 0);
    lv_label_set_text(label_inner_humidity, "---");
    lv_obj_set_pos(label_inner_humidity, -10, -10 + 90);
    lv_obj_set_style_text_align(label_inner_humidity, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);

    // ===============================右下角方块================================
    lv_obj_t *square2 = lv_obj_create(main_scr);
    lv_obj_set_size(square2, 116, 156);
    lv_obj_set_style_bg_color(square2, lv_color_hex(0xC0FCC0), 0);
    lv_obj_align(square2, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_radius(square2, 15, LV_PART_MAIN);
    lv_obj_clear_flag(square2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(square2, &style_focus, LV_STATE_FOCUSED);
    lv_group_add_obj(g, square2);
    lv_obj_add_event_cb(square2, square_event_cb, LV_EVENT_CLICKED, page_square2_display);

    lv_obj_t *label_shiwai = lv_label_create(square2);
    lv_obj_set_width(label_shiwai, 116);
    lv_obj_set_height(label_shiwai, 30);
    lv_obj_set_style_text_font(label_shiwai, &my_font_24, 0);
    lv_label_set_text(label_shiwai, "室外环境");
    lv_obj_set_pos(label_shiwai, -10, -10);
    lv_obj_set_style_text_align(label_shiwai, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    // 城市
    label_city = lv_label_create(square2);
    lv_obj_set_width(label_city, 116);
    lv_obj_set_height(label_city, 30);
    lv_obj_set_style_text_font(label_city, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_city, "City");
    lv_obj_set_pos(label_city, -10, -10 + 40);
    lv_obj_set_style_text_align(label_city, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    // ℃符号
    label_sheshidu = lv_label_create(square2);
    lv_obj_set_width(label_sheshidu, 112);
    lv_obj_set_height(label_sheshidu, 55);
    lv_obj_set_style_text_font(label_sheshidu, &my_font_24, 0);
    lv_label_set_text(label_sheshidu, "℃");
    lv_obj_set_pos(label_sheshidu, -10, -10 + 90);
    lv_obj_set_style_text_align(label_sheshidu, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    // 温度
    label_outdoor_temp = lv_label_create(square2);
    lv_obj_move_foreground(label_outdoor_temp);
    lv_obj_set_width(label_outdoor_temp, 112);
    lv_obj_set_height(label_outdoor_temp, 55);
    lv_obj_set_style_text_font(label_outdoor_temp, &lv_font_montserrat_52_partial, 0);
    lv_label_set_text(label_outdoor_temp, "---");
    lv_obj_set_pos(label_outdoor_temp, -10, -10 + 90);
    lv_obj_set_style_text_align(label_outdoor_temp, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);

#endif
#if (ENABLE_LVGL_USE == 0)
    ui_set_window(0, 0, 239, 319, BLACK);

    // 第一个矩形区域============================================================================
    ui_set_window(0, 0, 239, 155 - 1, CYAN);

    ui_draw_image(0, 0, &icon_wifi);
    int ssid_start_x =
        240 - 1 - (strlen(WIFI_SSID) + 1) * font20_maple_bold.height / 2;
    ui_write_string(ssid_start_x, 0, WIFI_SSID, &font20_maple_bold, CYAN,
                    BLACK);
    ui_write_string(240 - 1 - font20_maple_bold.height / 2, 0, "]",
                    &font20_maple_bold, CYAN, BLACK);
    ui_write_string(ssid_start_x - font20_maple_bold.height / 2, 0, "[",
                    &font20_maple_bold, CYAN, BLACK);

    ui_write_string(20, 30, "--:--", &font80_black, CYAN, BLACK);
    ui_write_string(18, 120, "----/--/-- 星期一", &font24_maple_bold, CYAN,
                    BLACK);

    // 第二个矩形区域============================================================================
    ui_set_window(0, 164, 114, 319, LIGHT_BLUE);
    // 固定的
    ui_write_string(0, 164, "室内环境", &font24_maple_bold, LIGHT_BLUE,
                    BLACK);
    ui_write_string(86, 164 + 54 - 7, "℃", &font24_maple_bold, LIGHT_BLUE,
                    BLACK);
    ui_write_string(88, 164 + 54 * 2 + 24 + 10 - 32 - 3, "%",
                    &font32_youyuan, LIGHT_BLUE, BLACK);

    // 变化的
    ui_write_string(0, 164 + 24, "---", &font54_songti, LIGHT_BLUE, BLACK);
    ui_write_string(0, 164 + 54 + 24 + 10, "---", &font54_songti,
                    LIGHT_BLUE, BLACK);

    // 第三个矩形区域============================================================================
    // 固定的
    ui_set_window(125, 164, 239, 319, LIGHT_GREEN);
    ui_draw_image(125 + 5, (239 + 319) / 2 - icon_wenduji.height / 2,
                  &icon_wenduji);
    ui_write_string(125 + 27 * 3 + 5, 164 + 54 - 7, "℃",
                    &font24_maple_bold, LIGHT_GREEN,
                    BLACK); // 终于236行
    // 变化的
    ui_write_string(125, 164, "City?", &font32_youyuan, LIGHT_GREEN,
                    BLACK); //
    ui_write_string(125, 164 + 24, "---", &font54_songti, LIGHT_GREEN,
                    BLACK); // 终于242行
    int start_y = (239 + 319) / 2 - icon99.height / 2;
    ui_draw_image(239 - icon99.width, start_y, &icon99); // icon图标居中
#endif
}
void main_page_redraw_wifissid(const char *wifi_ssid)
{
#if (ENABLE_LVGL_USE == 0)
    char str[30];
    ui_set_window(24, 0, 239, 24, CYAN);
    snprintf(str, sizeof(str), "%s", wifi_ssid);
    int start_x = 240 - 1 - (strlen(str) + 1) * font20_maple_bold.height / 2;
    ui_write_string(start_x, 0, str, &font20_maple_bold, CYAN, BLACK);
    ui_write_string(240 - 1 - font20_maple_bold.height / 2, 0, "]",
                    &font20_maple_bold, CYAN, BLACK);
    ui_write_string(start_x - font20_maple_bold.height / 2, 0, "[",
                    &font20_maple_bold, CYAN, BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (label_wifi_id != NULL)
    {
        if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
        {
            lv_label_set_text_fmt(label_wifi_id, "[%s]", wifi_ssid);
            xSemaphoreGive(xGuiMutex);
        }
    }
#endif
}
void main_page_redraw_time(rtc_time_t *rtc_time, bool is2sec, bool refresh)
{
#if (ENABLE_LVGL_USE == 0)
    if (refresh)
    {
        char time_str[6] = "\0";
        char separator = (rtc_time->second % 2 == 0) ? ' ' : ':';
        snprintf(time_str, sizeof(time_str), "%02d%c%02d", rtc_time->hour,
                 separator, rtc_time->minute);
        ui_write_string(20, 30, time_str, &font80_black, CYAN, BLACK);
    }
    else
    {
        if (is2sec)
            ui_write_string(20 + font80_black.height, 30, " ",
                            &font80_black, CYAN, BLACK);
        else
            ui_write_string(20 + font80_black.height, 30, ":",
                            &font80_black, CYAN, BLACK);
    }
#elif (ENABLE_LVGL_USE == 1)
    if (refresh)
    {
        char time_str[6] = "\0";
        char hour_str[3] = "\0";
        char min_str[3] = "\0";
        snprintf(time_str, sizeof(time_str), "%02d:%02d", rtc_time->hour, rtc_time->minute);
        memcpy(hour_str, time_str, 2);
        memcpy(min_str, time_str + 3, 2);
        if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
        {
            lv_label_set_text(label_hour, hour_str);
            lv_label_set_text(label_min, min_str);
            xSemaphoreGive(xGuiMutex);
        }
    }

    if (is2sec)
    {
        if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
        {
            lv_label_set_text(label_colon, ":");
            xSemaphoreGive(xGuiMutex);
        }
    }
    else
    {
        if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
        {
            lv_label_set_text(label_colon, "");
            xSemaphoreGive(xGuiMutex);
        }
    }
#endif
}
void main_page_redraw_date(rtc_time_t *rtc_time, bool refresh)
{
    char weekday[15];
    char str[20];

    switch (rtc_time->weekday)
    {
    case 1:
        strcpy(weekday, "星期一");
        break;
    case 2:
        strcpy(weekday, "星期二");
        break;
    case 3:
        strcpy(weekday, "星期三");
        break;
    case 4:
        strcpy(weekday, "星期四");
        break;
    case 5:
        strcpy(weekday, "星期五");
        break;
    case 6:
        strcpy(weekday, "星期六");
        break;
    case 7:
        strcpy(weekday, "星期日");
        break;
    default:
        strcpy(weekday, "X");
        break;
    }
    sprintf(str, "%04d/%02d/%02d %s", rtc_time->year, rtc_time->month,
            rtc_time->date, weekday);
#if (ENABLE_LVGL_USE == 0)
    ui_write_string(18, 120, str, &font24_maple_bold, CYAN, BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        lv_label_set_text(label_date, str);
        xSemaphoreGive(xGuiMutex);
    }
#endif
}
void main_page_redraw_inner_temperature(float temp)
{
    char temp_str[4] = {0}; // 符号位，两个温度，结束标志，总共4位
    if (temp > -20.0f && temp < 100.0f)
    {
        if (temp == 0)
            strcpy(temp_str, "  0");
        if (temp > 0)
            snprintf(temp_str, sizeof(temp_str), " %2f", temp);
        if (temp < 0)
            snprintf(temp_str, sizeof(temp_str), "%2f", temp);
    }
// printf("[Temperature] %s,%f\r\n", temp_str, temp);
#if (ENABLE_LVGL_USE == 0)
    ui_write_string(0, 164 + 24, temp_str, &font54_songti, LIGHT_BLUE,
                    BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        lv_label_set_text(label_inner_temp, temp_str);
        xSemaphoreGive(xGuiMutex);
    }
#endif
}
void main_page_redraw_inner_humidity(float humidity)
{
    char humidity_str[3];
    if (humidity > 0.0f && humidity < 99.99f)
    {
        if (humidity == 0)
            strcpy(humidity_str, "  0");
        else
            snprintf(humidity_str, sizeof(humidity_str), "%2f", humidity);
    }
    char tmp[5] = " ";
    strncat(tmp, humidity_str, sizeof(tmp));
#if (ENABLE_LVGL_USE == 0)
    ui_write_string(0, 164 + 54 + 24 + 10, tmp, &font54_songti, LIGHT_BLUE,
                    BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        lv_label_set_text(label_inner_humidity, tmp);
        xSemaphoreGive(xGuiMutex);
    }
#endif
}
void main_page_redraw_outdoor_city(const char *city)
{
    char str[9];
    snprintf(str, sizeof(str), "%-7s", city); // 右对齐，城市最多显示7个字母
#if (ENABLE_LVGL_USE == 0)
    ui_write_string(125, 164, str, &font32_youyuan, LIGHT_GREEN, BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        lv_label_set_text(label_city, str);
        xSemaphoreGive(xGuiMutex);
    }
#endif
}
void main_page_redraw_outdoor_temperature(char *temp)
{
    char temp_str[4];

    int temp_int = 0;
    sscanf(temp, "%d", &temp_int); // 先把温度变为整数;
    if (temp_int == 0)
        strcpy(temp_str, "  0");
    else if (temp_int > 0)
        snprintf(temp_str, sizeof(temp_str), "%d", temp_int);
    else
        snprintf(temp_str, sizeof(temp_str), "%d", temp_int);
    //"-18"  " -5"  "  6"  " 24"
    sprintf(temp_str, "%3d", temp_int);
#if (ENABLE_LVGL_USE == 0)
    ui_write_string(125, 164 + 24, temp_str, &font54_songti, LIGHT_GREEN,
                    BLACK);
#elif (ENABLE_LVGL_USE == 1)
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE)
    {
        lv_label_set_text(label_outdoor_temp, temp_str);
        xSemaphoreGive(xGuiMutex);
    }
#endif
}
void main_page_redraw_outdoor_weather_icon(char *text, bool isNight)
{
#if (ENABLE_LVGL_USE == 0)
    const img_t *icon;
#elif (ENABLE_LVGL_USE == 1)
    const lv_img_dsc_t *icon;
#endif
    if (strstr(text, "Sunny") != NULL) // 白天晴天
        icon = &icon0_1;
    else if (strstr(text, "Clear") != NULL) // 晚上晴天
        icon = &icon1;
    else if (strstr(text, "loudy") != NULL) // 多云或者部分地区多云
        icon = isNight ? &icon6 : &icon5;
    else if (strstr(text, "Overcast") != NULL) // 阴天
        icon = &icon9;
    else if (strstr(text, "hunder") != NULL) // 雷
        icon = &icon11;
    else if (strstr(text, "rain") != NULL || strstr(text, "lizzard") != NULL ||
             strstr(text, "drizzle") != NULL) // 雨
        icon = &icon13;
    else if (strstr(text, "snow") != NULL ||
             strstr(text, "snow") != NULL) // 雪天
        icon = &icon23;
    else if (strstr(text, "sleet") != NULL) // 雨夹雪
        icon = &icon20;
    else if (strstr(text, "Mist") != NULL || strstr(text, "og") != NULL) // 雾天
        icon = &icon31;
    else if (strstr(text, "pellet") != NULL) // 冻雨
        icon = &icon19;
    else
        icon = &icon99;
#if (ENABLE_LVGL_USE == 0)
    int start_y = 0;
    ui_set_window(240 - icon->width - 1, 320 - 80, 240, 320, LIGHT_GREEN);

    start_y = (239 + 319) / 2 - icon->height / 2;
    ui_draw_image(240 - icon->width - 1, start_y, icon);
#elif (ENABLE_LVGL_USE == 1)
#endif
}
