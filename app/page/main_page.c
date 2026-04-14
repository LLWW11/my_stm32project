#include "TFT_Img.h"
#include "TFT_LCD.h"
#include "app.h"
#include "esp_at.h"
#include "rtc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
void main_page_display(void)
{
    TFT_SetWindow(0, 0, 239, 319, BLACK);

    // 第一个矩形区域============================================================================
    TFT_SetWindow(0, 0, 239, 155 - 1, CYAN);

    TFT_LCD_show_img(0, 0, &icon_wifi);
    int ssid_start_x =
        240 - 1 - (strlen(WIFI_SSID) + 1) * font20_maple_bold.height / 2;
    TFT_LCD_Write_String(ssid_start_x, 0, WIFI_SSID, &font20_maple_bold, CYAN,
                         BLACK);
    TFT_LCD_Write_String(240 - 1 - font20_maple_bold.height / 2, 0, "]",
                         &font20_maple_bold, CYAN, BLACK);
    TFT_LCD_Write_String(ssid_start_x - font20_maple_bold.height / 2, 0, "[",
                         &font20_maple_bold, CYAN, BLACK);

    TFT_LCD_Write_String(20, 30, "--:--", &font80_black, CYAN, BLACK);
    TFT_LCD_Write_String(18, 120, "----/--/-- 星期一", &font24_maple_bold, CYAN,
                         BLACK);

    // 第二个矩形区域============================================================================
    TFT_SetWindow(0, 164, 114, 319, LIGHT_BLUE);
    // 固定的
    TFT_LCD_Write_String(0, 164, "室内环境", &font24_maple_bold, LIGHT_BLUE,
                         BLACK);
    TFT_LCD_Write_String(86, 164 + 54 - 7, "℃", &font24_maple_bold, LIGHT_BLUE,
                         BLACK);
    TFT_LCD_Write_String(88, 164 + 54 * 2 + 24 + 10 - 32 - 3, "%",
                         &font32_youyuan, LIGHT_BLUE, BLACK);

    // 变化的
    TFT_LCD_Write_String(0, 164 + 24, "---", &font54_songti, LIGHT_BLUE, BLACK);
    TFT_LCD_Write_String(0, 164 + 54 + 24 + 10, "---", &font54_songti,
                         LIGHT_BLUE, BLACK);

    // 第三个矩形区域============================================================================
    // 固定的
    TFT_SetWindow(125, 164, 239, 319, LIGHT_GREEN);
    TFT_LCD_show_img(125 + 5, (239 + 319) / 2 - icon_wenduji.height / 2,
                     &icon_wenduji);
    TFT_LCD_Write_String(125 + 27 * 3 + 5, 164 + 54 - 7, "℃",
                         &font24_maple_bold, LIGHT_GREEN,
                         BLACK); // 终于236行
    // 变化的
    TFT_LCD_Write_String(125, 164, "City?", &font32_youyuan, LIGHT_GREEN,
                         BLACK); //
    TFT_LCD_Write_String(125, 164 + 24, "---", &font54_songti, LIGHT_GREEN,
                         BLACK); // 终于242行
    int start_y = (239 + 319) / 2 - icon99.height / 2;
    TFT_LCD_show_img(239 - icon99.width, start_y, &icon99); // icon图标居中
}
void main_page_redraw_wifissid(const char *wifi_ssid)
{
    TFT_SetWindow(24, 0, 239, 24, CYAN);
    char str[21];
    snprintf(str, sizeof(str), "%s", wifi_ssid);
    int start_x = 240 - 1 - (strlen(str) + 1) * font20_maple_bold.height / 2;
    TFT_LCD_Write_String(start_x, 0, str, &font20_maple_bold, CYAN, BLACK);
    TFT_LCD_Write_String(240 - 1 - font20_maple_bold.height / 2, 0, "]",
                         &font20_maple_bold, CYAN, BLACK);
    TFT_LCD_Write_String(start_x - font20_maple_bold.height / 2, 0, "[",
                         &font20_maple_bold, CYAN, BLACK);
}
void main_page_redraw_time(rtc_time_t *rtc_time, bool is2sec, bool refresh)
{
    if (refresh)
    {
        char time_str[6] = "\0";
        char separator = (rtc_time->second % 2 == 0) ? ' ' : ':';
        snprintf(time_str, sizeof(time_str), "%02d%c%02d", rtc_time->hour,
                 separator, rtc_time->minute);
        TFT_LCD_Write_String(20, 30, time_str, &font80_black, CYAN, BLACK);
    }
    else
    {
        char separator = is2sec ? ' ' : ':';
        st7789_write_single_ascii(20 + font80_black.height, 30, separator,
                                  &font80_black, CYAN, BLACK);
    }
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
    TFT_LCD_Write_String(18, 120, str, &font24_maple_bold, CYAN, BLACK);
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
    TFT_LCD_Write_String(0, 164 + 24, temp_str, &font54_songti, LIGHT_BLUE,
                         BLACK);
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
    // printf("[Humidity] %s,%f\r\n", tmp, humidity);
    TFT_LCD_Write_String(0, 164 + 54 + 24 + 10, tmp, &font54_songti, LIGHT_BLUE,
                         BLACK);
}
void main_page_redraw_outdoor_city(const char *city)
{
    char str[9];
    snprintf(str, sizeof(str), "%-7s", city); // 右对齐，城市最多显示7个字母
    TFT_LCD_Write_String(125, 164, str, &font32_youyuan, LIGHT_GREEN, BLACK);
    // char str[9];
    // snprintf(str, sizeof(str), "%s", city);
    // TFT_LCD_Write_String(125, 164, str, &font24_maple_bold, LIGHT_GREEN,
    // BLACK);
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
    TFT_LCD_Write_String(125, 164 + 24, temp_str, &font54_songti, LIGHT_GREEN,
                         BLACK);
}
void main_page_redraw_outdoor_weather_icon(char *text, bool isNight)
{
    const img_t *icon;
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
    int start_y = 0;

    TFT_SetWindow(240 - icon->width - 1, 320 - 80, 240, 320, LIGHT_GREEN);

    start_y = (239 + 319) / 2 - icon->height / 2;
    TFT_LCD_show_img(240 - icon->width - 1, start_y, icon);
}
