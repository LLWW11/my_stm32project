#ifndef __BOARD_H
#define __BOARD_H

#include "board/led.h"

extern led_desc_t pled0;
extern led_desc_t pled1;

void board_low_level_init(void);
void board_Init(void);
#endif /*__BOARD_H*/
