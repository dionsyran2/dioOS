#include <drivers/rtc/rtc.h>

namespace RTC
{
#include <stdint.h>

    uint8_t read_cmos(uint8_t reg)
    {
        outb(0x70, reg);
        return inb(0x71);
    }

    bool is_updating()
    {
        outb(0x70, 0x0A);
        return (inb(0x71) & 0x80);
    }

    uint8_t bcd_to_bin(uint8_t val)
    {
        return (val & 0x0F) + ((val / 16) * 10);
    }

    rtc_time_t read_rtc_time()
    {
        rtc_time_t t1{}, t2{};

        // Wait until RTC is not updating
        do
        {
            while (is_updating());
            t1.second = read_cmos(0x00);
            t1.minute = read_cmos(0x02);
            t1.hour = read_cmos(0x04);
            t1.day_of_week = read_cmos(0x06);
            t1.day = read_cmos(0x07);
            t1.month = read_cmos(0x08);
            t1.year = read_cmos(0x09);
            t2 = t1;
            while (is_updating());
            t1.second = read_cmos(0x00);
            t1.minute = read_cmos(0x02);
            t1.hour = read_cmos(0x04);
            t1.day_of_week = read_cmos(0x06);
            t1.day = read_cmos(0x07);
            t1.month = read_cmos(0x08);
            t1.year = read_cmos(0x09);
        } while (memcmp(&t1, &t2, sizeof(rtc_time_t)) != 0); // Make sure values are stable

        // Check if values are in BCD
        outb(0x70, 0x0B);
        uint8_t regB = inb(0x71);

        if (!(regB & 0x04))
        { // BCD mode
            t1.second = bcd_to_bin(t1.second);
            t1.minute = bcd_to_bin(t1.minute);
            t1.hour = bcd_to_bin(t1.hour);
            t1.day_of_week = bcd_to_bin(t1.day_of_week);
            t1.day = bcd_to_bin(t1.day);
            t1.month = bcd_to_bin(t1.month);
            t1.year = bcd_to_bin(t1.year);
        }

        t1.year += 2000; // Or 1900 depending on BIOS; check CMOS 0x32 if available

        return t1;
    }

    const char* get_day_of_week(int day, int month, int year) {
        // If month is January or February, treat it as the 13th or 14th month of the previous year
        if (month < 3) {
            month += 12;
            year -= 1;
        }
    
        int K = year % 100; // Year of the century
        int J = year / 100; // Century
    
        // Apply Zeller's Congruence formula
        int h = (day + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 - 2 * J) % 7;
    
        // Map the result to the correct day of the week
        const char* days_of_week[] = {
            "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"
        };
    
        return days_of_week[h];
    }  
}