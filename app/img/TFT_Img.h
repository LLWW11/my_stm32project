#ifndef __TFT_IMG_H
#define __TFT_IMG_H

#include <stdint.h>

typedef struct
{
    uint16_t height;
    uint16_t width;
    const uint8_t *data;
} img_t;

extern const img_t img_laoba;
extern const img_t img_error;
extern const img_t img_wifi;
extern const img_t icon_wenduji;
extern const img_t icon_wifi;

extern const img_t icon0;
extern const img_t icon0_1;
extern const img_t icon1;
extern const img_t icon5;
extern const img_t icon6;
extern const img_t icon9;
extern const img_t icon11;
extern const img_t icon13;
extern const img_t icon19;
extern const img_t icon20;
extern const img_t icon23;
extern const img_t icon31;
extern const img_t icon99;

#endif /*__TFT_IMG_H*/
