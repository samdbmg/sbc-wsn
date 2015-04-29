/**
 * Real time clock configuration and control - header file
 */

#ifndef RTC_DRIVER_H_
#define RTC_DRIVER_H_

void rtc_init(void);
bool rtc_get_time_16(uint16_t* time_p);
void rtc_set_time(uint16_t timestamp, uint8_t msb);

void rtc_set_schedule(uint32_t period, uint32_t next_wake);

#endif /* RTC_DRIVER_H_ */
