#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include "rtc.h"

void rtc_init(void)
{
    RTC_InitTypeDef RTC_InitStructure;
    RTC_StructInit(&RTC_InitStructure);
    RTC_Init(&RTC_InitStructure);
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_TimeStampCmd(RTC_TimeStampEdge_Falling, ENABLE);
}

void rtc_set_time_once(rtc_time_t *rtc_time)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);

    date.RTC_Year = rtc_time->year - 2000;
    date.RTC_Month = rtc_time->month;
    date.RTC_WeekDay = rtc_time->weekday;
    date.RTC_Date = rtc_time->date;
    time.RTC_Hours = rtc_time->hour;
    time.RTC_Minutes = rtc_time->minute;
    time.RTC_Seconds = rtc_time->second;

    RTC_SetDate(RTC_Format_BIN, &date);
    RTC_SetTime(RTC_Format_BIN, &time);
}
void rtc_get_time_once(rtc_time_t *rtc_time)
{
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;

    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);

    RTC_GetDate(RTC_Format_BIN, &date);
    RTC_GetTime(RTC_Format_BIN, &time);

    rtc_time->year = 2000 + date.RTC_Year;
    rtc_time->month = date.RTC_Month;
    rtc_time->date = date.RTC_Date;
    rtc_time->weekday = date.RTC_WeekDay;
    rtc_time->hour = time.RTC_Hours;
    rtc_time->minute = time.RTC_Minutes;
    rtc_time->second = time.RTC_Seconds;
}
void rtc_set_time(rtc_time_t *rtc_time)
{
    rtc_set_time_once(rtc_time);
    // rtc_time_t time1, time2;
    // do
    // {
    //     rtc_set_time_once(&time1);
    //     rtc_set_time_once(&time2);
    // } while (memcmp(&time1, &time2, sizeof(time1)) != 0);
    // //防止记录时间的时候刚好出现进位，导致误差
}
void rtc_get_time(rtc_time_t *date_time)
{
    rtc_time_t time1, time2 = {0};
    do
    {
        rtc_get_time_once(&time1);
        rtc_get_time_once(&time2);
    } while (memcmp(&time1, &time2, sizeof(rtc_time_t)) != 0);
    // 读的时候同理
    memcpy(date_time, &time1, sizeof(rtc_time_t));
}