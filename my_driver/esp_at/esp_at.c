#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_at.h"
#include "board.h"
#include "delay.h"
#include "led.h"
#include "stm32f4xx.h"
#include "usart.h"
#include "tim.h"
#define RING_BUFFER_SIZE 1024

void usart2_sendByte(uint8_t data)
{
    USART_ClearFlag(USART2, USART_FLAG_TC);
    USART_SendData(USART2, data);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;
}
void usart2_sendString(uint8_t *str) // 发送字符串
{
    clear_usart_buffer();
    uint8_t i = 0;
    while (str[i] != '\0')
    {
        usart2_sendByte(str[i]);
        i++;
    }
}
// bool usart2_receiveByte_NonBlock(uint8_t *rx_data)
// {
//     if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == SET)
//     {
//         *rx_data = USART_ReceiveData(USART2);
//         return true;
//     }
//     else
//         return false;
// }
bool esp_at_WiFi_Init(void)
{
    char wifi_echo[100];
    memset(wifi_echo, 0, sizeof(wifi_echo));
    clear_usart_buffer();
    usart2_sendString("ATE0\r\n"); // 关闭回显，只返回结果
    usart2_sendString("AT+CWINIT?\r\n");
    int len1 = usart2_receiveString(wifi_echo, 100, 200);
    if (strstr(wifi_echo, "+CWINIT:1") != NULL || strstr(wifi_echo, "OK") != NULL)
    {
        led_reverse(pled1);
        memset(wifi_echo, 0, sizeof(wifi_echo));
        usart2_sendString("AT+CWMODE=1\r\n");
        len1 = usart2_receiveString(wifi_echo, 100, 20); // 设为连接路由器模式
        if (strstr(wifi_echo, "OK") != NULL)
        {
            led_reverse(pled1);
            return true;
        }
    }
    led_off(pled1);
    return false;
}
bool esp_connetWiFi(const char *ssid, const char *pwd, const char *mac)
{
    if (ssid == NULL || pwd == NULL)
        return false;
    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    if (mac != NULL) // 如果有mac
        snprintf(cmd + len, sizeof(cmd) - len, ",\"%s\"", mac);

    usart2_sendString((uint8_t *)cmd);
    return true;
}
/*
AT+CWSTATE?
+CWSTATE:2,"vivo50"

OK

AT+CWJAP?
+CWJAP:"vivo50","3a:c0:19:16:d4:00",11,-33,0,1,3,0,1

OK

{
    "results": [
        {
            "location": {
                "id": "WSSU6EXX52RE",
                "name": "Fuzhou",
                "country": "CN",
                "path": "Fuzhou,Fuzhou,Fujian,China",
                "timezone": "Asia/Shanghai",
                "timezone_offset": "+08:00"
            },
            "now": {
                "text": "Cloudy",
                "code": "4",
                "temperature": "14"
            },
            "last_update": "2026-03-09T16:10:19+08:00"
        }
    ]
}



* */
bool esp_at_http_get(const char *url, char *response, uint16_t response_len)
{
    char txbuff[256];
    snprintf(txbuff, sizeof(txbuff), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
    delay_ms(2);
    usart2_sendString((uint8_t *)txbuff);
    memset(response, 0, response_len);
    int echoLen = usart2_receiveString(response, response_len, 2000);
    return (echoLen != 0) ? true : false;
}
bool esp_at_sntp_Init(void)
{
    char echo[20];
    memset(echo, 0, 20);
    usart2_sendString("AT+CIPSNTPCFG=1,8\r\n"); // 设置东8区
    // delay_ms(2);
    usart2_receiveString(echo, 20, 2000);
    if (!strstr(echo, "+TIME_UPDATED"))
        return false;
    return true;
}
bool parse_CIPSNTPTIME(const char *response, time_Info_t *timeinfo) // 解析sntp时间
{
    response = strstr(response, "+CIPSNTPTIME:");
    response += strlen("+CIPSNTPTIME:");
    if (response == NULL)
        return false;
    memset(timeinfo, 0, sizeof(time_Info_t));
    /*
    +CIPSNTPTIME:Tue Mar 10 11:10:05 2026
    */
    uint8_t value = sscanf(response, "%s %s %d %d:%d:%d %s", timeinfo->weekday, timeinfo->month, &timeinfo->day, &timeinfo->hour, &timeinfo->min, &timeinfo->sec, timeinfo->year);
    if (value != 7)
        return false;
    return true;
}
bool parse_CWSTATE(const char *response, esp_wifi_info_t *info)
{
    response = strstr(response, "+CWSTATE:");
    if (response == NULL)
        return false;
    int wifi_state;
    if ((sscanf(response, "+CWSTATE:%d,\"%19[^\"]\"", &wifi_state, info->ssid) != 2))
        return false;
    info->isConnect = (wifi_state == 2);
    return true;
}
bool parse_CWJAP(const char *response, esp_wifi_info_t *info)
{
    response = strstr(response, "+CWJAP:");
    if (response == NULL)
        return false;
    int rssi, channel;
    if ((sscanf(response, "+CWJAP:\"%19[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->mac, channel, rssi) != 4))
        return false;

    return true;
}
bool parse_WeatherAPI(const char *response, weather_Info_t *info) // 解析WeatherAPI返回的额JSON数据
{
    // printf("=========%s=======\r\n", response);
    const char *location = strstr(response, "\"location\":");
    if (location == NULL)
        return false;
    // 提取城市信息
    const char *location_name = strstr(location, "\"name\":");
    if (location_name)
        sscanf(location_name, "\"name\": \"%31[^\"]", info->city);
    else
        strcpy(info->city, "ERROR");

    const char *location_path = strstr(location, "\"region\":");
    if (location_path)
        sscanf(location_path, "\"region\": \"%31[^\"]", info->location);
    else
        strcpy(info->location, "ERROR");

    // 提取温度信息
    const char *current = strstr(response, "\"current\":");

    if (current == NULL)
        return false;

    const char *now_text = strstr(current, "\"text\":");
    if (now_text)
        sscanf(now_text, "\"text\": \"%31[^\"]", info->weather);
    else
        strcpy(info->weather, "ERROR");

    const char *now_temp = strstr(current, "\"temp_c\":");
    if (now_temp)
    {
        float temp_c;
        if (sscanf(now_temp, "\"temp_c\": %f", &temp_c) == 1)
            sprintf(info->temperature, "%.1f", temp_c);
        else
            strcpy(info->temperature, "N/A");
    }
    else
        strcpy(info->temperature, "ERROR");

    const char *now_code = strstr(current, "\"code\":");
    if (now_code)
    {
        uint16_t temp_code;
        if (sscanf(now_code, "\"code\": %d", &temp_code) == 1)
            sprintf(info->weather_code, "%d", temp_code);
        else
            strcpy(info->weather_code, "N/A");
    }
    else
        strcpy(info->weather_code, "ERROR");
    return true;
}
bool parse_WeatherSenstive(const char *response, weather_Info_t *info) // 解析心知天气返回的额JSON数据
{
    response = strstr(response, "\"results\":");
    if (response == NULL)
        return false;
    const char *location = strstr(response, "\"location\":");
    if (location == NULL)
        return false;
    // 提取城市信息
    const char *location_name = strstr(location, "\"name\":");
    if (location_name)
        sscanf(location_name, "\"name\":\"%31[^\"]", info->city);
    else
        strcpy(info->city, "ERROR");
    const char *location_path = strstr(location, "\"path\":");
    if (location_path)
        sscanf(location_path, "\"path\":\"%63[^\"]", info->location);
    else
        strcpy(info->location, "ERROR");
    // 提取温度信息

    const char *now = strstr(response, "\"now\":");
    if (now == NULL)
        return false;
    const char *now_text = strstr(now, "\"text\":");
    if (now_text)
        sscanf(now_text, "\"text\":\"%31[^\"]", info->weather);
    else
        strcpy(info->weather, "ERROR");
    const char *now_code = strstr(now, "\"code\":");
    if (now_code)
    {
        sscanf(now_code, "\"code\":\"%7[^\"]", &info->weather_code);
    }
    else
        strcpy(info->weather_code, "ERROR");
    const char *now_temp = strstr(now, "\"temperature\":");
    if (now_temp)
        sscanf(now_temp, "\"temperature\":\"%7[^\"]", info->temperature);
    else
        strcpy(info->temperature, "ERROR");
    return true;
}
bool esp_get_wifi_info(esp_wifi_info_t *info)
{
    char echo[100];
    memset(echo, 0, 100);
    usart2_sendString("AT+CWSTATE?\r\n");
    if (usart2_receiveString(echo, 100, 100) == 0)
        return false;
    if (!parse_CWSTATE(echo, info))
        return false;
    if (info->isConnect == true)
    {
        memset(echo, 0, 100);
        usart2_sendString("AT+CWJAP?\r\n");
        if (usart2_receiveString(echo, 100, 100) == 0)
            return false;
        if (!parse_CWJAP(echo, info))
            return false;
    }
    return true;
}
bool esp_sntp_get(time_Info_t *timeinfo)
{
    char echo[100];
    // esp_at_sntp_Init();
    memset(echo, 0, sizeof(echo));
    usart2_sendString("AT+CIPSNTPTIME?\r\n");
    int len = usart2_receiveString(echo, sizeof(echo), 100);
    if (parse_CIPSNTPTIME(echo, timeinfo))
        return true;
    return false;
}