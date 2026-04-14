#include "app.h"
#include "board.h"
#include "page.h"
#include "delay.h"
/*
char str[] = "what are you doing now ?";
char buffer[1000] = {0};
char buffer1[1000] = {0};
uint8_t keynum = 0;
float temperature = 0;
float humidity = 0;
uint16_t len;
const char *weather_URL =
"http://api.weatherapi.com/v1/current.json?key=24f551dd292c4927850130628260903&q=auto:ip&lang=zh_cn";
esp_wifi_info_t wifi_Info;
time_Info_t timeinfo;
*/

int main()
{
    board_low_level_init();
    board_Init();
    welcome_page_display();
    wifi_init();
    wait_wifi_connet();
    // delay_ms(1000);
    main_page_display();
    main_loop_Init();
    while (1)
    {
        main_loop();
        /*
                if (KeyList[0].PressFlag == 1)
                {
                    KeyList[0].PressFlag = 0;
                    // ==================按键任务区=============================
                    keynum++;
                    printf("KEY0 Pressed!\r\n");
                    memset(buffer, 0, sizeof(buffer));
                    usart2_sendString("AT+CWSTATE?\r\n");
                    len = usart2_receiveString(buffer, sizeof(buffer), 50);
                    if (parse_CWSTATE(buffer, &wifi_Info))
                        printf("wifi state:2,[%s]\r\n", wifi_Info.ssid);
                    printf("Original Data: %s\r\n", buffer);
                }
                if (KeyList[1].PressFlag == 1)
                {
                    KeyList[1].PressFlag = 0;
                    printf("KEY1 Pressed!\r\n");
                    keynum++;
                    esp_at_sntp_Init();
                    usart2_sendString("AT+CIPSNTPTIME?\r\n");
                    memset(buffer1, 0, sizeof(buffer1));
                    len = usart2_receiveString(buffer1, sizeof(buffer1), 100);
                    if (parse_CIPSNTPTIME(buffer1, &timeinfo))
                    {
                        printf("[Year]: \r\t%s\r\n", timeinfo.year);
                        printf("[Month]: \r\t%s\r\n", timeinfo.month);
                        printf("[Date]: \r\t%d\r\n", timeinfo.day);
                        printf("[Time]: \r\t%02u:%02u:%02u\r\n", timeinfo.hour,
           timeinfo.min, timeinfo.sec); printf("[Weekday]:\r\t%s\r\n",
           timeinfo.weekday);
                    }
                    printf("Original Data:  %s\r\n", buffer1);
                }
                if (KeyList[2].PressFlag == 1)
                {
                    KeyList[2].PressFlag = 0;
                    printf("KEY2 Pressed!\r\n");
                    keynum++;
                    weather_Info_t weather_info;
                    char weather_http_response[1000];
                    // uint8_t lenurl = ;
                    while (!esp_at_http_get(weather_URL, weather_http_response,
           1000))
                        ;
                    parse_WeatherAPI(weather_http_response, &weather_info);
                    // parse_WeatherSenstive(weather_http_response,
           &weather_info); printf("%s\n", weather_http_response);

                    printf("current city:\r\t%s\r\n", weather_info.city);
                    printf("location:\r\t%s\r\n", weather_info.location);
                    printf("weather text:\r\t%s\r\n", weather_info.weather);
                    printf("weather code:\r\t%s\r\n",
           weather_info.weather_code); printf("temperature:\r\t%s\r\n",
           weather_info.temperature); if (aht20_start_measurement())
                        printf("AHT20 start measuring\r\n");
                    delay_ms(80);
                    aht20_read_measurement(&temperature, &humidity);
                    printf("temperature: %2f,humidity: %2f\r\n", temperature,
           humidity);
                }
                keynum %= 2;

                */
    }
}
