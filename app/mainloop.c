#include "AHT20.h"
#include "TFT_LCD.h"
#include "app.h"
#include "board.h"
#include "delay.h"
#include "esp_at.h"
#include "key.h"
#include "page.h"
#include "usart.h"
#include <string.h>

#define MS(x)      (x)
#define SECONDS(x) MS((x) * 1000)
#define MINUTES(x) SECONDS((x) * 60)
#define HOURS(x)   MINUTES((x) * 60)
#define DAYS(x)    HOURS((x) * 24)

#define TIME_SYNC_INTERVAL      DAYS(1)
#define WIFI_UPDATE_INTERNAL    SECONDS(5)
#define TIME_UPDATE_INTERNAL    SECONDS(1)
#define INNER_UPDATE_INTERNAL   MINUTES(1)
#define OUTDOOR_UPDATE_INTERNAL MINUTES(2)

static uint32_t time_sync_delay;
static uint32_t wifi_update_delay;
static uint32_t time_update_delay;
static uint32_t inner_update_delay;
static uint32_t outdoor_update_delay;

bool isNight = false;
bool is2sec = false;
bool refresh = false;

static void cpu_periodic_callback(void)
{
    if (time_sync_delay > 0)
        time_sync_delay--;
    if (wifi_update_delay > 0)
        wifi_update_delay--;
    if (time_update_delay > 0)
        time_update_delay--;
    if (inner_update_delay > 0)
        inner_update_delay--;
    if (outdoor_update_delay > 0)
        outdoor_update_delay--;
}
static void time_sync()
{
    if (time_sync_delay > 0)
        return;
    time_sync_delay = TIME_SYNC_INTERVAL;
    time_Info_t esp_time;
    memset(&esp_time, 0, sizeof(esp_time));
    if (!esp_sntp_get(&esp_time))
    {
        printf("[SNTP] get time failed\n");
        time_sync_delay = SECONDS(5); // 间隔5s钟再试一次
        return;
    }
    uint16_t year = 0;
    uint8_t weekday = 0;
    uint8_t month = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        char c = esp_time.year[i];
        if (c < '0' || c > '9')
            break;
        year = (c - '0') + year * 10;
    }

    if (strstr(esp_time.month, "Jan") != NULL)
        month = 1;
    else if (strstr(esp_time.month, "Feb") != NULL)
        month = 2;
    else if (strstr(esp_time.month, "Mar") != NULL)
        month = 3;
    else if (strstr(esp_time.month, "Apr") != NULL)
        month = 4;
    else if (strstr(esp_time.month, "May") != NULL)
        month = 5;
    else if (strstr(esp_time.month, "Jun") != NULL)
        month = 6;
    else if (strstr(esp_time.month, "Jul") != NULL)
        month = 7;
    else if (strstr(esp_time.month, "Aug") != NULL)
        month = 8;
    else if (strstr(esp_time.month, "Sep") != NULL)
        month = 9;
    else if (strstr(esp_time.month, "Oct") != NULL)
        month = 10;
    else if (strstr(esp_time.month, "Nov") != NULL)
        month = 11;
    else if (strstr(esp_time.month, "Dec") != NULL)
        month = 12;
    else
        month = 1; //

    if (strstr(esp_time.weekday, "Mon") != NULL)
        weekday = 1;
    else if (strstr(esp_time.weekday, "Tue") != NULL)
        weekday = 2;
    else if (strstr(esp_time.weekday, "Wed") != NULL)
        weekday = 3;
    else if (strstr(esp_time.weekday, "Thu") != NULL)
        weekday = 4;
    else if (strstr(esp_time.weekday, "Fri") != NULL)
        weekday = 5;
    else if (strstr(esp_time.weekday, "Sat") != NULL)
        weekday = 6;
    else if (strstr(esp_time.weekday, "Sun") != NULL)
        weekday = 7;
    if (weekday == 0)
        weekday = 1;
    if (year < 2000)
    {
        printf("[SNTP] invalid date formate\r\n");
        time_sync_delay = SECONDS(5); // 间隔5s钟再试一次
        return;
    }
    printf("[SNTP] time:%04u-%02u-%02u %02u:%02u:%02u %s\n", year, month,
           esp_time.day, esp_time.hour, esp_time.min, esp_time.sec,
           esp_time.weekday);

    rtc_time_t rtc_date = {0};
    rtc_date.year = year;
    rtc_date.month = month;
    rtc_date.weekday = weekday;
    rtc_date.date = esp_time.day;
    rtc_date.hour = esp_time.hour;
    rtc_date.minute = esp_time.min;
    rtc_date.second = esp_time.sec;
    rtc_set_time(&rtc_date);

    time_update_delay = 0;
}
static void wifi_update(void)
{
    static esp_wifi_info_t last_Info = {0};
    if (wifi_update_delay > 0)
        return;
    wifi_update_delay = WIFI_UPDATE_INTERNAL;
    static esp_wifi_info_t wifiInfo;
    memset(&wifiInfo, 0, sizeof(wifiInfo));
    if (!esp_get_wifi_info(&wifiInfo))
    {
        printf("[AT] wifi info get failed\n");
        return;
    }
    if (memcmp(&wifiInfo, &last_Info, sizeof(esp_wifi_info_t)) == 0)
        return;

    if (last_Info.isConnect == wifiInfo.isConnect)
        return;
    if (wifiInfo.isConnect)
    {
        printf("[WIFI] connected to %s\n", wifiInfo.ssid);
        printf("[WIFI] SSID: %s, MAC: %s,  RSSI: %d\n", wifiInfo.ssid,
               wifiInfo.mac, wifiInfo.rssi);
        main_page_redraw_wifissid(wifiInfo.ssid);
    }
    else
    {
        printf("[WIFI] disconnected from %s\n", last_Info.ssid);
        main_page_redraw_wifissid("No WiFi Conneted");
    }
    memcpy(&last_Info, &wifiInfo, sizeof(esp_wifi_info_t));
}
static void time_update(void)
{
    static rtc_time_t last_time = {0};

    if (time_update_delay > 0)
        return;

    time_update_delay = TIME_UPDATE_INTERNAL;

    rtc_time_t current_time;
    rtc_get_time(&current_time);

    if (current_time.year < 2020)
        return;

    if (memcmp(&current_time, &last_time, sizeof(rtc_time_t)) == 0)
        return;

    isNight = (current_time.hour >= 18) ? true : false;
    is2sec = (current_time.second % 2 == 0) ? true : false;
    refresh = (current_time.minute == last_time.minute) ? false : true;

    memcpy(&last_time, &current_time, sizeof(rtc_time_t));

    main_page_redraw_time(&current_time, is2sec, refresh);
    main_page_redraw_date(&current_time, refresh);
}
static void inner_update(void)
{
    static float last_temperature, last_humidity;

    if (inner_update_delay > 0)
        return;

    inner_update_delay = INNER_UPDATE_INTERNAL;

    if (!aht20_start_measurement())
    {
        printf("[AHT20] start measurement failed\n");
        return;
    }
    if (!aht20_wait_for_measurement())
    {
        printf("[AHT20] wait for measurement failed\n");
        return;
    }

    float temperature = 0.0f, humidity = 0.0f;

    if (!aht20_read_measurement(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        return;
    }
    if (temperature == last_temperature && humidity == last_humidity)
        return;

    last_temperature = temperature;
    last_humidity = humidity;

    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature,
           humidity);
    main_page_redraw_inner_temperature(temperature);
    main_page_redraw_inner_humidity(humidity);
}
bool outdoor_update(void)
{
    static weather_Info_t last_weather = {0};
    uint8_t retry_cnt = 0;
    if (outdoor_update_delay > 0)
        return false;

    outdoor_update_delay = OUTDOOR_UPDATE_INTERNAL;

    weather_Info_t weather = {0};
    const char *weather_url =
        "http://api.weatherapi.com/v1/current.json?key=24f551dd292c4927850130628260903&q=auto:ip&lang=zh_cn";
    char weather_http_response[1000];
    if (!esp_at_http_get(weather_url, weather_http_response,
                         sizeof(weather_http_response)))
    {
        printf("[WEATHER] http error\n");
        return false;
    }
    while (strstr(weather_http_response, "busy") != NULL)
    {
        memset(weather_http_response, 0, sizeof(weather_http_response));
        clear_usart_buffer();
        esp_at_http_get(weather_url, weather_http_response, sizeof(weather_http_response));
        retry_cnt++;
        printf("[Weather Retry] %d\n", retry_cnt);
        if (retry_cnt > 3)
            break;
    }
    retry_cnt = 0;
    if (!parse_WeatherAPI(weather_http_response, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return false;
    }
    if (memcmp(&last_weather, &weather, sizeof(weather_Info_t)) == 0)
    {
        printf("[WEATHER] Info: %s, %s, temp: %s \n", weather.city,
               weather.weather, weather.temperature);
        return false;
    }

    main_page_redraw_outdoor_temperature(weather.temperature);
    main_page_redraw_outdoor_weather_icon(weather.weather, isNight);
    if (strcmp(last_weather.city, weather.city) != 0)
        main_page_redraw_outdoor_city(weather.city);

    memcpy(&last_weather, &weather, sizeof(weather_Info_t));
    printf("[WEATHER] UI update %s, %s, temp: %s \n", weather.city,
           weather.weather, weather.temperature);
    return true;
}

void main_loop_Init(void)
{
    cpu_register_periodic_callback(cpu_periodic_callback);
}

void main_loop(void)
{
    time_sync();
    wifi_update();
    time_update();
    inner_update();
    outdoor_update();
}