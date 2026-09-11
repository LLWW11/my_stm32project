/**
 * @file lv_port_disp.h
 * @brief LVGL 显示驱动适配层 —— 对接 ST7789 TFT LCD
 *
 * 使用方法：
 *   在 LVGL 初始化中调用 lv_port_disp_init()
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 显示驱动
 *        - 分配 draw buffer
 *        - 注册 flush_cb，将 LVGL 渲染完的像素通过 DMA 发送给 ST7789
 *
 * 必须在 lv_init() 之后调用，在创建任何 LVGL 控件之前调用。
 */
void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
