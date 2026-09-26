#ifndef __TFT_LCD_H
#define __TFT_LCD_H
#include "TFT_Img.h"
#include "font.h"
#include "dbg_config.h"
// 基本颜色
#define RED   0XF800 // 红色
#define GREEN 0X07E0 // 绿色
#define BLUE  0X001F // 蓝色
#define WHITE 0XFFFF // 白色
#define BLACK 0X0000 // 黑色

#define CYAN        0x07FF // 青色
#define YELLOW      0xFFE0 // 黄色
#define PINK        0xFE19 // 粉色
#define RICE        0xF7BB // 米色
#define LIGHT_BLUE  0xAEBC
#define LIGHT_GREEN 0xC7F8

#define mkcolor(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define TFT_COLUMN_NUMBER 240
#define TFT_LINE_NUMBER   320

void TFT_init(void);
#if (ENABLE_LVGL_USE == 0)
void TFT_SetWindow(uint16_t sx,
                   uint16_t sy,
                   uint16_t ex,
                   uint16_t ey,
                   uint16_t color);
void TFT_LCD_Write_String(uint16_t x,
                          uint16_t y,
                          char *str,
                          const font_t *font,
                          uint16_t color_bg,
                          uint16_t color_ch);
void st7789_write_single_ascii(uint16_t x, // 起始x坐标
                               uint16_t y, // 起始Y坐标
                               char ch,    // 要显示的字符
                               const font_t *font,
                               uint16_t color_bg,
                               uint16_t color_ch);
void TFT_LCD_show_img(uint16_t x, uint16_t y, const img_t *img);
#endif
#if (ENABLE_LVGL_USE == 1)
void TFT_Color_Buffer(uint16_t x1,
                      uint16_t y1,
                      uint16_t x2,
                      uint16_t y2,
                      const uint16_t *color_p);

#endif
#endif /*__TFT_LCD_H*/
