#ifndef __FONT_H
#define __FONT_H

// !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~

#include <stdint.h>
typedef struct
{
    const char *name;     // 汉字
    const uint8_t *model; // 汉字对应的字模
} Ch_font_t;

typedef struct
{
    uint16_t height; // ASCII：宽度是高度的一半，汉字：宽度等于高度
    const uint8_t *ascii_model;
    // const char *ascii_map;
    const Ch_font_t *chinese;
    // uint16_t size;
} font_t;

extern const font_t font32_youyuan;
extern const font_t font20_maple_bold;
extern const font_t font24_maple_bold;
extern const font_t font80_black;
extern const font_t font54_songti;
#endif /*__FONT_H*/