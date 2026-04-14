#ifndef __LED_H
#define __LED_H

#include <stdint.h>
#include <stdbool.h>

struct led_desc;

typedef struct led_desc *led_desc_t;

/*	通用的方法	*/
void LED_Init(void);
void LED_ON(void);

/*	面向对象的方法	*/
void led_init(led_desc_t led);
void led_init_PWM(void);
void led_set(led_desc_t led, bool onoff);
void led_on(led_desc_t led);
void led_off(led_desc_t led);
void led_all_off(void);
void led_reverse(led_desc_t led);
void led0_pwm(void);
#endif /*__LED_H*/
