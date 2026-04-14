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
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

bool is_system_ready = false;

#define MLOOP_EVENT_TIME_SYNC      (1 << 0)
#define MLOOP_EVENT_WIFI_UPDATE    (1 << 2)
#define MLOOP_EVENT_TIME_UPDATE    (1 << 3)
#define MLOOP_EVENT_INNER_UPDATE   (1 << 4)
#define MLOOP_EVENT_OUTDOOR_UPDATE (1 << 5)
#define MLOOP_EVENT_ALL            (MLOOP_EVENT_TIME_SYNC |    \
                                    MLOOP_EVENT_WIFI_UPDATE |  \
                                    MLOOP_EVENT_TIME_UPDATE |  \
                                    MLOOP_EVENT_INNER_UPDATE | \
                                    MLOOP_EVENT_OUTDOOR_UPDATE)

#define MS(x)      (x)
#define SECONDS(x) MS((x) * 1000)
#define MINUTES(x) SECONDS((x) * 60)
#define HOURS(x)   MINUTES((x) * 60)
#define DAYS(x)    HOURS((x) * 24)

#define TIME_SYNC_INTERVAL      HOURS(1)
#define WIFI_UPDATE_INTERVAL    SECONDS(5)
#define TIME_UPDATE_INTERVAL    SECONDS(1)
#define INNER_UPDATE_INTERVAL   MINUTES(1)
#define OUTDOOR_UPDATE_INTERVAL MINUTES(2)

static TaskHandle_t mloop_task;
static TimerHandle_t time_sync_timer;
static TimerHandle_t wifi_update_timer;
static TimerHandle_t time_update_timer;
static TimerHandle_t inner_update_timer;
static TimerHandle_t outdoor_update_timer;

bool isNight = false;
bool is2sec = false;
bool refresh = false;

static void time_sync(void)
{
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;
    time_Info_t esp_time;
    rtc_time_t rtc_date = {0};
    uint16_t year = 0;
    uint8_t weekday = 0;
    uint8_t month = 0;
    memset(&esp_time, 0, sizeof(esp_time));

    if (!esp_at_sntp_get_time(&esp_time))
    {
#if ENABLE_DEBUG_PRINT
        printf("[SNTP] get time failed\n");
#endif
        restart_sync_delay = SECONDS(1);
        goto err;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        char c = esp_time.year[i];
        if (c < '0' || c > '9')
            break;
        year = (c - '0') + year * 10; // rtc的年份转化为字符串
    }
    const char *month_tmp[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; i++)
    {
        if (strstr(esp_time.month, month_tmp[i]) != NULL)
            month = i + 1;
    }
    /*
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
            month = 1; */
    const char *weekday_tmp[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (uint8_t i = 0; i < 12; i++)
    {
        if (strstr(esp_time.weekday, weekday_tmp[i]) != NULL)
            weekday = i + 1;
    }
    /*
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
        */
    if (year < 2000)
    {
#if ENABLE_DEBUG_PRINT
        printf("[SNTP] invalid date format\r\n");
#endif
        restart_sync_delay = SECONDS(1);
        goto err;
    }
#if ENABLE_DEBUG_PRINT
    printf("[SNTP] time:%04u-%02u-%02u %02u:%02u:%02u %s\n", year, month,
           esp_time.day, esp_time.hour, esp_time.min, esp_time.sec,
           esp_time.weekday);
#endif
    rtc_date.year = year;
    rtc_date.month = month;
    rtc_date.weekday = weekday;
    rtc_date.date = esp_time.day;
    rtc_date.hour = esp_time.hour;
    rtc_date.minute = esp_time.min;
    rtc_date.second = esp_time.sec;
    rtc_set_time(&rtc_date);

err:
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay), 0);
    // xTaskNotify(mloop_task, MLOOP_EVENT_TIME_UPDATE, eSetBits);
}
static void wifi_update(void)
{
    static esp_wifi_info_t last_Info = {0};
    static esp_wifi_info_t wifiInfo;
    if (!is_system_ready)
        return;
    xTimerChangePeriod(wifi_update_timer, pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), 0);
    memset(&wifiInfo, 0, sizeof(wifiInfo));
    if (!esp_at_get_wifi_info(&wifiInfo))
    {
#if ENABLE_DEBUG_PRINT
        printf("[AT] wifi info get failed\n");
#endif
        return;
    }
    if (memcmp(&wifiInfo, &last_Info, sizeof(esp_wifi_info_t)) == 0)
        return;

    if (last_Info.isConnect == wifiInfo.isConnect)
        return;
    if (wifiInfo.isConnect)
    {
#if ENABLE_DEBUG_PRINT
        printf("[WIFI] connected to %s\n", wifiInfo.ssid);
        printf("[WIFI] SSID: %s, MAC: %s,  RSSI: %d\n", wifiInfo.ssid,
               wifiInfo.mac, wifiInfo.rssi);
#endif
        main_page_redraw_wifissid(wifiInfo.ssid);
    }
    else
    {
#if ENABLE_DEBUG_PRINT
        printf("[WIFI] disconnected from %s\n", last_Info.ssid);
#endif
        main_page_redraw_wifissid("No WiFi Conneted");
    }
    memcpy(&last_Info, &wifiInfo, sizeof(esp_wifi_info_t));
    // xTimerChangePeriod(wifi_update_timer, pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), 0);
}
static void time_update(void)
{
    static rtc_time_t last_time = {0};
    xTimerChangePeriod(time_update_timer, pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), 0);
    rtc_time_t current_time;
    rtc_get_time(&current_time);

    if (current_time.year < 2020)
        return;

    if (memcmp(&current_time, &last_time, sizeof(rtc_time_t)) == 0)
        return;

    isNight = (current_time.hour >= 18) ? true : false;
    is2sec = (current_time.second % 2 == 0) ? true : false;
    refresh = (current_time.minute == last_time.minute) ? false : true;
    if (!is_system_ready)
        return;
    memcpy(&last_time, &current_time, sizeof(rtc_time_t));

    main_page_redraw_time(&current_time, is2sec, refresh);
    main_page_redraw_date(&current_time, refresh);
}
static void inner_update(void)
{
    static float last_temperature, last_humidity;
    xTimerChangePeriod(inner_update_timer, pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), 0);
    if (!is_system_ready)
        return;
    if (!aht20_start_measurement())
    {
#if ENABLE_DEBUG_PRINT
        printf("[AHT20] start measurement failed\n");
#endif
        return;
    }
    if (!aht20_wait_for_measurement())
    {
#if ENABLE_DEBUG_PRINT
        printf("[AHT20] wait for measurement failed\n");
#endif
        return;
    }

    float temperature = 0.0f, humidity = 0.0f;

    if (!aht20_read_measurement(&temperature, &humidity))
    {
#if ENABLE_DEBUG_PRINT
        printf("[AHT20] read measurement failed\n");
#endif
        return;
    }
    if (temperature == last_temperature && humidity == last_humidity)
        return;

    last_temperature = temperature;
    last_humidity = humidity;
#if ENABLE_DEBUG_PRINT
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature,
           humidity);
#endif
    main_page_redraw_inner_temperature(temperature);
    main_page_redraw_inner_humidity(humidity);
}
static void outdoor_update(void)
{
    const char *weather_url = "https://api.weatherapi.com/v1/current.json?key=24f551dd292c4927850130628260903&q=auto:ip&lang=zh_cn";

    // xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);
    static weather_Info_t last_weather = {0};

    weather_Info_t weather = {0};
    const char *weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
#if ENABLE_DEBUG_PRINT
        printf("[WEATHER] http error\n");
#endif
        uint32_t delay = is_system_ready ? OUTDOOR_UPDATE_INTERVAL : SECONDS(5);
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(delay), 0);
        return;
    }
    if (!parse_WeatherAPI(weather_http_response, &weather))
    {
#if ENABLE_DEBUG_PRINT
        printf("[WEATHER] parse failed\n");
#endif
        uint32_t delay = is_system_ready ? OUTDOOR_UPDATE_INTERVAL : SECONDS(5);
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(delay), 0);
        return;
    }
    xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);

    if (!is_system_ready)
    {
        is_system_ready = true; // 系统初始化完毕
        main_page_display();
        xTaskNotify(mloop_task, MLOOP_EVENT_TIME_UPDATE | MLOOP_EVENT_INNER_UPDATE | MLOOP_EVENT_WIFI_UPDATE, eSetBits);
    }

    if (memcmp(&last_weather, &weather, sizeof(weather_Info_t)) == 0)
        return;

    memcpy(&last_weather, &weather, sizeof(weather_Info_t));
#if ENABLE_DEBUG_PRINT
    printf("[WEATHER] %s, %s, %s\n", weather.city, weather.weather, weather.temperature);
#endif
    main_page_redraw_outdoor_temperature(weather.temperature);
    main_page_redraw_outdoor_weather_icon(weather.weather, isNight);
    // if (strcmp(last_weather.city, weather.city) != 0)
    main_page_redraw_outdoor_city(weather.city);

    memcpy(&last_weather, &weather, sizeof(weather_Info_t));
#if ENABLE_DEBUG_PRINT
    printf("[WEATHER] UI update: %s, %s, temp: %s \n", weather.city,
           weather.weather, weather.temperature);
#endif
    // return true;
}
/*
bool outdoor_update(void)
{
    static weather_Info_t last_weather = {0};
    // uint8_t retry_cnt = 0;
    // xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);

    weather_Info_t weather = {0};
    const char weather_http_response[1000];

    if (esp_at_http_get(weather_url) == NULL)
    {
        printf("[WEATHER] http error\n");
        return false;
    }
    if (strstr(weather_http_response, "busy") != NULL)
    {
        memset(weather_http_response, 0, sizeof(weather_http_response));
        clear_usart_buffer();
        esp_at_http_get(weather_url, weather_http_response, sizeof(weather_http_response));
        // retry_cnt++;
        printf("[Weather Retry] %d\n", retry_cnt);
    }
    // retry_cnt = 0;
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
*/
void main_loop(void)
{
    time_sync();
    wifi_update();
    time_update();
    inner_update();
    outdoor_update();
}

static void mloop_func(void *param)
{
    uint32_t event;
    while (1)
    {
        event = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (event & MLOOP_EVENT_TIME_SYNC)
            time_sync();
        if (event & MLOOP_EVENT_WIFI_UPDATE)
            wifi_update();
        if (event & MLOOP_EVENT_TIME_UPDATE)
            time_update();
        if (event & MLOOP_EVENT_INNER_UPDATE)
            inner_update();
        if (event & MLOOP_EVENT_OUTDOOR_UPDATE)
            outdoor_update();
    }
}

static void mloop_timer_callback(TimerHandle_t time1)
{
    uint32_t event = (uint32_t)pvTimerGetTimerID(time1);
    xTaskNotify(mloop_task, event, eSetBits);
}
static void time_update_callback(TimerHandle_t time1)
{
    time_update();
}
void main_loop_Init(void)
{
    time_sync_timer = xTimerCreate("time_sync",
                                   1,
                                   pdFALSE,
                                   (void *)MLOOP_EVENT_TIME_SYNC,
                                   mloop_timer_callback);

    time_update_timer = xTimerCreate("time update",
                                     pdMS_TO_TICKS(TIME_UPDATE_INTERVAL),
                                     pdTRUE,
                                     NULL,
                                     time_update_callback);

    // time_update_timer = xTimerCreate("time_update", pdMS_TO_TICKS(1), pdFALSE, (void *)MLOOP_EVENT_TIME_UPDATE, mloop_timer_callback);
    wifi_update_timer = xTimerCreate("wifi_update",
                                     pdMS_TO_TICKS(1),
                                     pdTRUE,
                                     (void *)MLOOP_EVENT_WIFI_UPDATE,
                                     mloop_timer_callback);
    inner_update_timer = xTimerCreate("inner_update",
                                      pdMS_TO_TICKS(1),
                                      pdTRUE,
                                      (void *)MLOOP_EVENT_INNER_UPDATE,
                                      mloop_timer_callback);

    outdoor_update_timer = xTimerCreate("outdoor_update",
                                        pdMS_TO_TICKS(1),
                                        pdTRUE,
                                        (void *)MLOOP_EVENT_OUTDOOR_UPDATE,
                                        mloop_timer_callback);

    xTaskCreate(mloop_func, "mloop", 4096, NULL, 5, &mloop_task);

    xTaskNotify(mloop_task, MLOOP_EVENT_ALL, eSetBits);

    xTimerStart(wifi_update_timer, 0);
    xTimerStart(time_update_timer, 0);
    xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);
}
