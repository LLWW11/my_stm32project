#include "FreeRTOS.h"
#include "app.h"
#include "app_ui.h"
#include "board.h"
#include "page.h"
#include "task.h"
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
esp_wifi_info_t wifi_Info; time_Info_t timeinfo;
*/

static void main_init(void *param)
{
    board_Init();
    UI_init();
    welcome_page_display();
    wifi_init();

    wait_wifi_connet();
    // main_page_display();//放在outdoor_update里面等待天气更新完成后才显示
    main_loop_Init();
    vTaskDelete(NULL);
}

int main()
{
    board_low_level_init();
    xTaskCreate(main_init, "main_init", 1024, NULL, 9, NULL);
    vTaskStartScheduler();
    while (1)
    {
        ; // Code shouldn't run here
    }
}

void vAssertCalled(const char *file, int line)
{
    while (1)
    {
        ;
    }
}