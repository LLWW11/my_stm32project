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
#if (ENABLE_LVGL_USE == 1)
LV_FONT_DECLARE(lv_font_montserrat_52_partial);
LV_FONT_DECLARE(my_font_24);

static void time_update_timer_cb(lv_timer_t *timer)
{
    rtc_time_t rtc_time;
    rtc_get_time(&rtc_time);
    lv_obj_t *label = (lv_obj_t *)timer->user_data;
    if (label == NULL)
        return;

    char time_str[15];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", rtc_time.hour, rtc_time.minute, rtc_time.second);
    lv_label_set_text(label, time_str);
}
static void square0_del_cb(lv_event_t *e)
{
    lv_timer_t *timer = (lv_timer_t *)lv_event_get_user_data(e);
    if (timer)
    {
        lv_timer_del(timer);
    }
}
void page_square0_display(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    rtc_time_t rtc_time;
    rtc_get_time(&rtc_time);
    lv_obj_t *label_current_time = lv_label_create(parent);
    lv_obj_set_size(label_current_time, LV_PCT(100), 60);
    lv_obj_set_style_text_font(label_current_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_current_time, lv_color_hex(0x000000), 0);
    lv_obj_set_pos(label_current_time, 0, 250);
    lv_obj_set_style_text_align(label_current_time, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", rtc_time.hour, rtc_time.minute, rtc_time.second);
    lv_label_set_text(label_current_time, time_str);

    lv_timer_t *timer = lv_timer_create(time_update_timer_cb, 1000, label_current_time);
    lv_obj_add_event_cb(parent, square0_del_cb, LV_EVENT_DELETE, timer);

    lv_obj_t *calendar = lv_calendar_create(parent);
    lv_obj_set_size(calendar, LV_PCT(100), 240);
    lv_obj_align(calendar, LV_ALIGN_TOP_MID, 0, 0);
    /* 设置当前日期  */
    lv_calendar_set_today_date(calendar, rtc_time.year, rtc_time.month, rtc_time.date);
    lv_calendar_set_showed_date(calendar, rtc_time.year, rtc_time.month);
    lv_calendar_header_dropdown_create(calendar); // 设置日历头
    lv_obj_update_layout(calendar);               // 更新日历参数
}

void page_square1_display(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    float temp = 0.0f, hum = 0.0f;
    get_current_inner_env(&temp, &hum);

    // Title
    lv_obj_t *label_title = lv_label_create(parent);
    lv_obj_set_style_text_font(label_title, &my_font_24, 0);
    lv_label_set_text(label_title, "室内环境");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);

    // Create Arc for Temperature
    lv_obj_t *arc_temp = lv_arc_create(parent);
    lv_arc_set_rotation(arc_temp, 135);
    lv_arc_set_bg_angles(arc_temp, 0, 270);
    lv_arc_set_value(arc_temp, (int)temp);
    lv_arc_set_range(arc_temp, -10, 50);
    lv_obj_set_size(arc_temp, 100, 100);
    lv_obj_align(arc_temp, LV_ALIGN_LEFT_MID, 0, 10);
    lv_obj_remove_style(arc_temp, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_temp, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label_temp_val = lv_label_create(arc_temp);
    lv_label_set_text_fmt(label_temp_val, "%.1f C", temp);
    lv_obj_center(label_temp_val);

    lv_obj_t *label_temp_text = lv_label_create(parent);
    lv_label_set_text(label_temp_text, "Temp");
    lv_obj_align_to(label_temp_text, arc_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_t *arc_hum = lv_arc_create(parent);
    lv_arc_set_rotation(arc_hum, 135);
    lv_arc_set_bg_angles(arc_hum, 0, 270);
    lv_arc_set_value(arc_hum, (int)hum);
    lv_arc_set_range(arc_hum, 0, 100);
    lv_obj_set_size(arc_hum, 100, 100);
    lv_obj_align(arc_hum, LV_ALIGN_RIGHT_MID, 0, 10);
    lv_obj_remove_style(arc_hum, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_hum, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label_hum_val = lv_label_create(arc_hum);
    lv_label_set_text_fmt(label_hum_val, "%.1f %%", hum);
    lv_obj_center(label_hum_val);

    lv_obj_t *label_hum_text = lv_label_create(parent);
    lv_label_set_text(label_hum_text, "Humid");
    lv_obj_align_to(label_hum_text, arc_hum, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}
void page_square2_display(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    weather_Info_t weather_info;
    get_current_weather_info(&weather_info);

    lv_obj_t *label_title = lv_label_create(parent);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_30, 0);
    lv_label_set_text(label_title, "Detail");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, -10);

    lv_obj_t *label_city = lv_label_create(parent);
    lv_obj_set_style_text_font(label_city, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(label_city, "City: %s", weather_info.city);
    lv_obj_align(label_city, LV_ALIGN_TOP_LEFT, 10, 30);

    lv_obj_t *label_loc = lv_label_create(parent);
    lv_obj_set_style_text_font(label_loc, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(label_loc, "Location: %s", weather_info.location);
    lv_obj_align(label_loc, LV_ALIGN_TOP_LEFT, 10, 60);

    lv_obj_t *label_wea = lv_label_create(parent);
    lv_obj_set_style_text_font(label_wea, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(label_wea, "Weather: %s", weather_info.weather);
    lv_obj_align(label_wea, LV_ALIGN_TOP_LEFT, 10, 90);

    lv_obj_t *label_code = lv_label_create(parent);
    lv_obj_set_style_text_font(label_code, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(label_code, "Code: %s", weather_info.weather_code);
    lv_obj_align(label_code, LV_ALIGN_TOP_LEFT, 10, 120);

    lv_obj_t *label_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(label_temp, "Temp: %s ", weather_info.temperature);
    lv_obj_align(label_temp, LV_ALIGN_TOP_LEFT, 10, 150);

    lv_obj_t *label_sheshidu = lv_label_create(parent);
    lv_obj_set_style_text_font(label_sheshidu, &my_font_24, 0);
    lv_label_set_text(label_sheshidu, "℃");
    lv_obj_align_to(label_sheshidu, label_temp, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    rtc_time_t current_time;
    rtc_get_time(&current_time);
    bool isNight = (current_time.hour >= 18) ? true : false;

    const char *text = weather_info.weather;
    const lv_img_dsc_t *icon;
    if (strstr(text, "Sunny") != NULL)
        icon = &icon0_1;
    else if (strstr(text, "Clear") != NULL)
        icon = &icon1;
    else if (strstr(text, "loudy") != NULL)
        icon = isNight ? &icon6 : &icon5;
    else if (strstr(text, "Overcast") != NULL)
        icon = &icon9;
    else if (strstr(text, "hunder") != NULL)
        icon = &icon11;
    else if (strstr(text, "rain") != NULL || strstr(text, "lizzard") != NULL || strstr(text, "drizzle") != NULL)
        icon = &icon13;
    else if (strstr(text, "snow") != NULL)
        icon = &icon23;
    else if (strstr(text, "sleet") != NULL)
        icon = &icon20;
    else if (strstr(text, "Mist") != NULL || strstr(text, "og") != NULL)
        icon = &icon31;
    else if (strstr(text, "pellet") != NULL)
        icon = &icon19;
    else
        icon = &icon99;

    lv_obj_t *img_icon = lv_img_create(parent);
    // lv_img_set_zoom(img_icon, 128);
    lv_img_set_src(img_icon, icon);
    // lv_obj_align_to(img_icon, label_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_align(img_icon, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *img_wenduji = lv_img_create(parent);
    // lv_img_set_zoom(img_icon, 128);
    lv_img_set_src(img_wenduji, &icon_wenduji);
    // lv_obj_align_to(img_icon, label_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_align(img_wenduji, LV_ALIGN_CENTER, 100, 20);
}
#endif