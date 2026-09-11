#ifndef __APP_UI_H
#define __APP_UI_H

#include <stdint.h>
#include "esp_at.h"
#include "font.h"
#include "TFT_Img.h"
#include "dbg_config.h"
#include "FreeRTOS.h"
#include "semphr.h"
void UI_init(void);

#if (ENABLE_LVGL_USE == 0)
void UI_init(void);
void ui_write_string(uint16_t x,
                     uint16_t y,
                     const char *str,
                     const font_t *font,
                     uint16_t color_ch,
                     uint16_t color_bg);
void ui_set_window(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void ui_draw_image(uint16_t x, uint16_t y, const img_t *image);
#endif

#if (ENABLE_LVGL_USE == 1)
#include "lvgl.h"

extern SemaphoreHandle_t xGuiMutex;
extern lv_indev_t *keypad_indev;
void app_keypad_init(void);
void get_current_weather_info(weather_Info_t *info);
void get_current_inner_env(float *temp, float *hum);
// 需要动态更新的控件
extern lv_obj_t *label_hour;
extern lv_obj_t *label_colon;
extern lv_obj_t *label_min;
extern lv_obj_t *label_city;
extern lv_obj_t *label_wifi_id;
extern lv_obj_t *label_time;           // 时间标签
extern lv_obj_t *label_date;           // 日期标签
extern lv_obj_t *label_inner_temp;     // 室内温度标签
extern lv_obj_t *label_outdoor_temp;   // 室外温度标签
extern lv_obj_t *label_inner_humidity; // 室内温度标签
extern lv_obj_t *img_weather_icon;     // 天气图标
extern lv_obj_t *img_wifi_icon;        // 图标
#endif

#endif /*__APP_UI_H*/