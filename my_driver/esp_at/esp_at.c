#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "esp_at.h"
#include "board.h"
#include "led.h"
#include "stm32f4xx.h"
#include "usart.h"
#include "dma.h"
#include "tim.h"

#define RING_BUFFER_SIZE 1024
// 1.0是裸机版本，2.0是FreeRTOS的版本
#if (version == 2)

#define ESP_AT_DEBUG    0
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
typedef enum
{
    AT_ACK_NONE,
    AT_ACK_OK,
    AT_ACK_ERROR,
    AT_ACK_BUSY,
    AT_ACK_READY,
} at_ack_t;
typedef struct
{
    at_ack_t ack;
    const char *string;
} at_ack_match_t;
static const at_ack_match_t at_ack_matches[] =
    {
        {AT_ACK_OK, "OK\r\n"},
        {AT_ACK_ERROR, "ERROR\r\n"},
        {AT_ACK_BUSY, "busy p\r\n"},
        {AT_ACK_READY, "ready\r\n"},
};

static at_ack_t rxack;
static SemaphoreHandle_t at_ack_sempahore;
static char *rxline;
static char rxbuf[2048];
static uint32_t rxlen;

static bool esp_at_write_command(const char *command, uint32_t timeout);
static bool esp_at_wait_boot(uint32_t timeout);
static bool esp_at_wait_ready(uint32_t timeout);

static at_ack_t match_internal_ack(const char *str)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)
    {
        if (strstr(str, at_ack_matches[i].string) != NULL)
            return at_ack_matches[i].ack;
    }
    return AT_ACK_NONE;
}
static void esp_at_usart_write(const char *data)
{
    uint32_t len = strlen(data);

    DMA_Cmd(DMA1_Stream6, DISABLE);

    while (DMA_GetCmdStatus(DMA1_Stream6) != DISABLE)
        ;
    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = len;

    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);
    DMA_Cmd(DMA1_Stream6, ENABLE);
}
static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)
{
    rxlen = 0;
    rxline = rxbuf;
    bool acked = xSemaphoreTake(at_ack_sempahore, pdMS_TO_TICKS(timeout)) == pdPASS;
    return acked ? rxack : AT_ACK_NONE;
}
static bool esp_at_wait_ready(uint32_t timeout)
{
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}
static void esp_at_lowlevel_init(void)
{
    usart2_Init();
    ESP_DMA_Init();
}
bool esp_at_init(void)
{
    at_ack_sempahore = xSemaphoreCreateBinary();
    configASSERT(at_ack_sempahore);

    esp_at_lowlevel_init();

    if (!esp_at_wait_boot(3000))
        return false;
    if (!esp_at_write_command("AT+RESTORE\r\n", 2000))
        return false;
    if (!esp_at_wait_ready(5000))
        return false;

    return true;
}
static bool esp_at_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", command);
#endif

    esp_at_usart_write(command);
    at_ack_t ack = esp_at_usart_wait_receive(timeout);

#if ESP_AT_DEBUG
    printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif

    return ack == AT_ACK_OK;
}
static const char *esp_at_get_response(void)
{
    return rxbuf;
}
static bool esp_at_wait_boot(uint32_t timeout)
{
    for (int t = 0; t < timeout; t += 100)
    {
        if (esp_at_write_command("AT\r\n", 100))
            return true;
    }

    return false;
}
bool esp_at_WiFi_Init(void)
{
    return esp_at_write_command("AT+CWMODE=1\r\n", 2000);
}
bool esp_at_connect_wifi(const char *ssid, const char *pwd, const char *mac)
{
    if (ssid == NULL || pwd == NULL)
        return false;

    char *cmd = rxbuf;
    int len = snprintf(cmd, sizeof(rxbuf), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    if (mac)
        snprintf(cmd + len, sizeof(rxbuf) - len, ",\"%s\"", mac);

    return esp_at_write_command(cmd, 5000);
}
static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)
{
    //    AT+CWSTATE?
    //    +CWSTATE:2,"Xiaomi Mi MIX 3_5577"

    //    OK
    response = strstr(response, "+CWSTATE:");
    if (response == NULL)
        return false;

    int wifi_state;
    if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
        return false;

    info->isConnect = (wifi_state == 2);

    return true;
}
static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)
{
    response = strstr(response, "+CWJAP:");
    if (response == NULL)
        return false;

    if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d", info->ssid, info->mac, &info->rssi) != 3)
        return false;

    return true;
}
bool esp_at_get_wifi_info(esp_wifi_info_t *info)
{
    if (!esp_at_write_command("AT+CWSTATE?\r\n", 2000))
        return false;

    if (!parse_cwstate_response(esp_at_get_response(), info))
        return false;

    if (info->isConnect == true)
    {
        if (!esp_at_write_command("AT+CWJAP?\r\n", 2000))
            return false;

        if (!parse_cwjap_response(esp_at_get_response(), info))
            return false;
    }

    return true;
}
bool wifi_is_connected(void)
{
    esp_wifi_info_t info;
    if (esp_at_get_wifi_info(&info))
        return info.isConnect;
    return false;
}
bool esp_at_sntp_Init(void)
{
    if (!esp_at_write_command("AT+CIPSNTPCFG=1,8\r\n", 2000))
        return false;

    return true;
}
static bool parse_cipsntptime_response(const char *response, time_Info_t *date)
{
    //	AT+CIPSNTPTIME?
    //	+CIPSNTPTIME:Sun Jul 27 14:07:19 2025
    //	OK
    char weekday_str[8];
    char month_str[4];
    response = strstr(response, "+CIPSNTPTIME:");
    if (sscanf(response, "+CIPSNTPTIME:%3s %3s %hhu %hhu:%hhu:%hhu %4s",
               weekday_str, month_str,
               &date->day, &date->hour, &date->min, &date->sec, &date->year) != 7)
        return false;

    strcpy(date->weekday, weekday_str);
    strcpy(date->month, month_str);
    // date->weekday = weekday_str;
    // date->month = month_str;

    return true;
}
bool esp_at_sntp_get_time(time_Info_t *date)
{
    if (!esp_at_write_command("AT+CIPSNTPTIME?\r\n", 2000))
        return false;

    if (!parse_cipsntptime_response(esp_at_get_response(), date))
        return false;

    return true;
}
const char *esp_at_http_get(const char *url)
{
    //    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=SfRic8Wmp-Qh3OeFk&location=WTEMH46Z5N09&language=en&unit=c",,,2
    //    +HTTPCLIENT:261,{"results":[{"location":{"id":"WTEMH46Z5N09","name":"Hefei","country":"CN","path":"Hefei,Hefei,Anhui,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Cloudy","code":"4","temperature":"32"},"last_update":"2025-07-26T16:30:00+08:00"}]}

    //    OK
    // char *txbuf = rxbuf;
    static char txbuf[256];
    bool ret;
    ret = esp_at_write_command("ATE0\r\n", 5000);
    // bool ret;
    // snprintf(txbuf, sizeof(txbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
    snprintf(txbuf, sizeof(txbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
    ret = esp_at_write_command(txbuf, 5000);
    // printf("%s\r\n", rxbuf);
    return ret ? rxbuf : NULL;
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET || USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART2);
        if (rxlen < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen++] = USART_ReceiveData(USART2);
            if (rxbuf[rxlen - 1] == '\n')
            {
                rxbuf[rxlen] = '\0';
                if ((rxbuf + rxlen - rxline) < 20)
                {
                    at_ack_t ack = match_internal_ack(rxline);
                    if (ack != AT_ACK_NONE)
                    {
                        rxack = ack;
                        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
                        xSemaphoreGiveFromISR(at_ack_sempahore, &pxHigherPriorityTaskWoken);
                        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
                    }
                }
                rxline = rxbuf + rxlen;
            }
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

#endif

#if (version == 1)

void usart2_sendByte(uint8_t data)
{
    USART_ClearFlag(USART2, USART_FLAG_TC);
    USART_SendData(USART2, data);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;
}
void usart2_sendString(uint8_t *str)
{
    clear_usart_buffer();
    uint8_t i = 0;
    while (str[i] != '\0')
    {
        usart2_sendByte(str[i]);
        i++;
    }
}
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
bool esp_at_http_get(const char *url, char *response, uint16_t response_len)
{
    char txbuff[256];
    snprintf(txbuff, sizeof(txbuff), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
    vTaskDelay(pdMS_TO_TICKS(2));
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
void clear_usart_buffer(void)
{
    rx_buffer.head = 0;
    rx_buffer.tail = 0;
}

void ring_buffer_push(uint8_t data)
{
    uint16_t next_head = (rx_buffer.head + 1) % RING_BUFFER_SIZE;
    if (next_head != rx_buffer.tail)
    {
        rx_buffer.buffer[rx_buffer.head] = data;
        rx_buffer.head = next_head;
    }
}
bool ring_buffer_pop(uint8_t *data)
{
    if (rx_buffer.head == rx_buffer.tail)
    {
        return false;
    }
    *data = rx_buffer.buffer[rx_buffer.tail];
    rx_buffer.tail = (rx_buffer.tail + 1) % RING_BUFFER_SIZE;
    return true;
}
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART2);
        ring_buffer_push(rx_data);
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

#endif

bool parse_WeatherAPI(const char *response, weather_Info_t *info) // 解析WeatherAPI返回的额JSON数据
{
#if ESP_AT_DEBUG
    printf("=========%s=======\r\n", response);
#endif
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
        int temp_code;
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