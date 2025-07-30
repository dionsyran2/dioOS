#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../../IO.h"
#include "../../memory.h"

#define RTC_PORT 0x70
#define RTC_DATA 0x71

#define RTC_STATUS_REGISTER 0x0A
#define RTC_UPDATE_IN_PROGRESS 0x80

namespace RTC
{
    struct rtc_time_t
    {
        uint32_t msec;
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t day_of_week;
        uint8_t month;
        uint16_t year;
    };

    rtc_time_t read_rtc_time();
    const char *get_day_of_week(int day, int month, int year);
}