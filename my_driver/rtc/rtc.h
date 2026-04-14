#ifndef __RTC_H
#define __RTC_H

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} rtc_time_t;

void rtc_init(void);
void rtc_set_time(rtc_time_t *rtc_time);
void rtc_get_time(rtc_time_t *rtc_time);

#endif /*__USART_H*/
