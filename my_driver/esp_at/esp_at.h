#ifndef __ESP_AT_H
#define __ESP_AT_H
#include <stdbool.h>

volatile extern uint16_t esp_timeout;
typedef struct
{
    const char ssid[20];
    const char mac[18];
    const char pwd[20];
    int rssi;
    bool isConnect;
} esp_wifi_info_t;

typedef struct
{
    char city[32];
    char location[64];
    char weather[32];
    char weather_code[8];
    char temperature[8];
} weather_Info_t;

typedef struct
{
    char year[5];
    char month[3];
    int day;
    int hour;
    int min;
    int sec;
    char weekday[5];
     //   +CIPSNTPTIME:Tue Mar 10 11:10:05 2026
} time_Info_t;

void usart2_Init(void); // usart2和esp32通信
void usart2_sendByte(uint8_t data);
void usart2_sendString(uint8_t *str);
bool usart2_receiveByte_NonBlock(uint8_t *rx_data);
uint16_t usart2_receiveString(char *buffer, uint16_t bufferSize, uint32_t timeout);

bool esp_at_WiFi_Init(void);
bool esp_connetWiFi(const char *ssid, const char *pwd, const char *mac);
bool parse_CWSTATE(const char *response, esp_wifi_info_t *info);
bool parse_CWJAP(const char *response, esp_wifi_info_t *info);
bool esp_at_http_get(const char *url, char *response, uint16_t response_len);
bool esp_at_sntp_Init(void);
bool parse_CIPSNTPTIME(const char *response, time_Info_t *timeinfo);
bool parse_WeatherSenstive(const char *response, weather_Info_t *info);
bool parse_WeatherAPI(const char *response, weather_Info_t *info);
bool esp_get_wifi_info(esp_wifi_info_t *info);
bool esp_sntp_get(time_Info_t *timeinfo);
#endif /*__ESP_AT_H*/
