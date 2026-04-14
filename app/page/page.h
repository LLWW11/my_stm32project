#ifndef __PAGE_H
#define __PAGE_H
#include "rtc.h"
#include "stdbool.h"
void welcome_page_display(void);
void error_page_display(const char *msg);
void wifi_page_display(void);
void main_page_display(void);
void main_page_redraw_wifissid(const char *wifi_ssid);
void main_page_redraw_time(rtc_time_t *rtc_time, bool is2sec, bool refresh);
void main_page_redraw_date(rtc_time_t *rtc_time, bool refresh);
void main_page_redraw_inner_temperature(float temp);
void main_page_redraw_inner_humidity(float humidity);
void main_page_redraw_outdoor_city(const char *city);
void main_page_redraw_outdoor_temperature(char *temp);
void main_page_redraw_outdoor_weather_icon(char *text, bool isNight);
#endif /*__PAGE_H*/