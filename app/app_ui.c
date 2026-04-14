#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app_ui.h"
#include "TFT_LCD.h"
#include "font.h"
#include "TFT_Img.h"
#include "dbg_config.h"
typedef enum
{
    UI_ACTION_SET_WINDOW,
    UI_ACTION_WRITE_STRING,
    UI_ACTION_DRAW_IMAGE,
} ui_action_t;

typedef struct
{
    ui_action_t act;
    union
    {
        struct
        {
            uint16_t sx;
            uint16_t sy;
            uint16_t ex;
            uint16_t ey;
            uint16_t color;
        } set_window;
        struct
        {
            uint16_t x;
            uint16_t y;
            char *str;
            const font_t *font;
            uint16_t color_bg;
            uint16_t color_ch;
        } write_string;
        struct
        {
            uint16_t x;
            uint16_t y;
            const img_t *img;
        } draw_image;
    };
} UI_msg_t;

static QueueHandle_t ui_queue;

static void UI_func(void *param)
{
    UI_msg_t ui_msg;
    TFT_init();
    while (1)
    {
        xQueueReceive(ui_queue, &ui_msg, portMAX_DELAY);
        switch (ui_msg.act)
        {
        case UI_ACTION_SET_WINDOW:
            TFT_SetWindow(ui_msg.set_window.sx,
                          ui_msg.set_window.sy,
                          ui_msg.set_window.ex,
                          ui_msg.set_window.ey,
                          ui_msg.set_window.color);
            break;
        case UI_ACTION_WRITE_STRING:
            TFT_LCD_Write_String(ui_msg.write_string.x,
                                 ui_msg.write_string.y,
                                 ui_msg.write_string.str,
                                 ui_msg.write_string.font,
                                 ui_msg.write_string.color_bg,
                                 ui_msg.write_string.color_ch);
            vPortFree((void *)ui_msg.write_string.str);
            break;
        case UI_ACTION_DRAW_IMAGE:
            TFT_LCD_show_img(ui_msg.draw_image.x,
                             ui_msg.draw_image.y,
                             ui_msg.draw_image.img);
            break;
        default:
#if ENABLE_DEBUG_PRINT
            printf("[DEBUG] Unknown UI action:%d\r\n", ui_msg.act);
#endif
            break;
        }
    }
}

void UI_init(void)
{
    ui_queue = xQueueCreate(16, sizeof(UI_msg_t));
    configASSERT(ui_queue);
    xTaskCreate(UI_func, "UI", 2048, NULL, 8, NULL);
}

void ui_set_window(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    UI_msg_t ui_msg;
    ui_msg.act = UI_ACTION_SET_WINDOW;
    ui_msg.set_window.sx = sx;
    ui_msg.set_window.sy = sy;
    ui_msg.set_window.ex = ex;
    ui_msg.set_window.ey = ey;
    ui_msg.set_window.color = color;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);
}

void ui_write_string(uint16_t x,
                     uint16_t y,
                     const char *str,
                     const font_t *font,
                     uint16_t color_bg,
                     uint16_t color_ch)
{
    char *pstr = pvPortMalloc(strlen(str) + 1);
    if (pstr == NULL)
    {
#if ENABLE_DEBUG_PRINT
        printf("ui write string malloc failed: %s", str);
#endif
        return;
    }
    strcpy(pstr, str);

    UI_msg_t ui_msg;
    ui_msg.act = UI_ACTION_WRITE_STRING;
    ui_msg.write_string.x = x;
    ui_msg.write_string.y = y;
    ui_msg.write_string.str = pstr;
    ui_msg.write_string.color_ch = color_ch;
    ui_msg.write_string.color_bg = color_bg;
    ui_msg.write_string.font = font;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);
}

void ui_draw_image(uint16_t x, uint16_t y, const img_t *image)
{
    UI_msg_t ui_msg;
    ui_msg.act = UI_ACTION_DRAW_IMAGE;
    ui_msg.draw_image.x = x;
    ui_msg.draw_image.y = y;
    ui_msg.draw_image.img = image;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);
}
