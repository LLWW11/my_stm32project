#ifndef __TFT_IMG_H
#define __TFT_IMG_H

#include <stdint.h>
#include "dbg_config.h"

typedef struct
{
    uint16_t height;
    uint16_t width;
    const uint8_t *data;
} img_t;
#if (ENABLE_LVGL_USE == 0)
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


#elif (ENABLE_LVGL_USE == 1)
/*  头文件包含  */
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
#ifndef LV_ATTRIBUTE_IMG_IMG_WIFI
#define LV_ATTRIBUTE_IMG_IMG_WIFI
#endif

/*  变量声明  */
extern const lv_img_dsc_t img_wifi;
extern const lv_img_dsc_t img_meihua;
extern const lv_img_dsc_t icon_wifi;
extern const lv_img_dsc_t img_error;
extern const lv_img_dsc_t icon_wenduji;
extern const lv_img_dsc_t icon0_1;
extern const lv_img_dsc_t icon1;
extern const lv_img_dsc_t icon5;
extern const lv_img_dsc_t icon6;
extern const lv_img_dsc_t icon9;
extern const lv_img_dsc_t icon11;
extern const lv_img_dsc_t icon13;
extern const lv_img_dsc_t icon23;
extern const lv_img_dsc_t icon20;
extern const lv_img_dsc_t icon31;
extern const lv_img_dsc_t icon19;
extern const lv_img_dsc_t icon99;

#endif
#endif /*__TFT_IMG_H*/
