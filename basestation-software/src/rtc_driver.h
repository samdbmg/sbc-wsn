/**
 * Activate the real time clock and use it to trigger periodic interrupts - header
 *
 */

#ifndef RTC_DRIVER_H_
#define RTC_DRIVER_H_

void rtc_init(void);

void rtc_set(uint8_t hour, uint8_t minute, uint8_t second, uint8_t year,
        uint8_t month, uint8_t day);

uint32_t rtc_get_time_of_day(void);

void rtc_schedule_callback(void (*fn)(void), uint32_t time);

#endif /* RTC_DRIVER_H_ */
