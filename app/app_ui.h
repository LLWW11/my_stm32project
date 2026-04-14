#ifndef __APP_UI_H
#define __APP_UI_H

#include <stdint.h>
#include "font.h"
#include "TFT_Img.h"

void UI_init(void);
void ui_write_string(uint16_t x,
                     uint16_t y,
                     const char *str,
                     const font_t *font,
                     uint16_t color_ch,
                     uint16_t color_bg);
void ui_set_window(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void ui_draw_image(uint16_t x, uint16_t y, const img_t *image);

#endif /*__APP_UI_H*/