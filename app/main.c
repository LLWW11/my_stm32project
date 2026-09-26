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
#include "w25q128_test.h"
#include "can_iap.h"
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
        { // 独占互斥锁最多10ms
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

#define CAN_TEST_LOOPBACK 0

// loopback与双机测试的任务
static void can_test_task(void *argument)
{
    CanRxMsg rx;
    const uint8_t probe[4] = {0x4C, 0x4F, 0x4F, 0x50}; /* "LOOP" */
    uint8_t seq = 0;

    (void)argument;

    if (!can_test_init(CAN_TEST_LOOPBACK != 0))
    {
        printf("CAN init failed\r\n");
        vTaskDelete(NULL);
    }

    while (1)
    {
#if CAN_TEST_LOOPBACK
        bool sent = can_test_send(0x320, probe, sizeof(probe),
                                  pdMS_TO_TICKS(100));
        bool received = sent && can_test_receive(&rx, pdMS_TO_TICKS(100));

        if (received && rx.StdId == 0x320U &&
            rx.DLC == sizeof(probe) &&
            memcmp(rx.Data, probe, sizeof(probe)) == 0)
            printf("CAN loopback OK\r\n");
        else
            printf("CAN loopback FAIL\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
#else
        uint8_t heartbeat[1] = {seq++};

        /* STM32 每两秒发 0x320；同时轮询 i.MX6ULL 的 0x321 */
        if (!can_test_send(0x320, heartbeat, sizeof(heartbeat),
                           pdMS_TO_TICKS(100)))
            printf("CAN 0x320 TX failed\r\n");

        for (uint16_t i = 0; i < 100U; ++i)
        {
            if (can_test_receive(&rx, pdMS_TO_TICKS(20)) &&
                rx.StdId == 0x321U)
            {
                printf("CAN 0x321 RX, DLC=%u\r\n", rx.DLC);

                /* 收到 0x321 后，以 0x322 原样回复其数据 */
                if (!can_test_send(0x322, rx.Data, rx.DLC, pdMS_TO_TICKS(100)))
                    printf("CAN 0x322 TX failed\r\n");
            }
        }
#endif
    }
}

static void main_init(void *param)
{
    board_Init();
    UI_init();
    lv_port_disp_init();
#if (ENABLE_LVGL_USE == 1)
    app_keypad_init();
#endif
    /*
    //测试用
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_center(label);
*/

    welcome_page_display();
    xTaskCreate(can_test_task, "can_test", 512, NULL, 7, NULL);
    vTaskDelete(NULL); /* 仅用于 CAN 联调版；通过后恢复原业务流程。 */
    wifi_init();
    w25q128_self_test(); // w25q128测试用
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