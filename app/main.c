#include "FreeRTOS.h"
#include "app.h"
#include "app_ui.h"
#include "board.h"
#include "page.h"
#include "task.h"
#include "dbg_config.h"
#include "lvgl.h"
#include "TFT_LCD.h"
#include "lv_port_disp.h"
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
#if (ENABLE_LVGL_USE == 1)
static void lvgl_task(void *pvParameter)
{
    printf("LVGL run before\n");
    while (1)
    {
        if (xGuiMutex != NULL)
        {
            if (xSemaphoreTake(xGuiMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                lv_task_handler();
                xSemaphoreGive(xGuiMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif
static void main_init(void *param)
{
    board_Init();
    UI_init();
    lv_port_disp_init();
#if (ENABLE_LVGL_USE == 1)
    app_keypad_init();
#endif
    /*
    //≤‚ ‘”√
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_center(label);
*/

    welcome_page_display();
    wifi_init();

    wait_wifi_connet();
    main_loop_Init();
    vTaskDelete(NULL);
}

int main()
{
    board_low_level_init();
    xTaskCreate(main_init, "main_init", 1024, NULL, 9, NULL);
#if (ENABLE_LVGL_USE)
    xTaskCreate(lvgl_task, "lvgl_task", 1024, NULL, 8, NULL);
#endif
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
void vApplicationTickHook(void)
{
    lv_tick_inc(1);
}