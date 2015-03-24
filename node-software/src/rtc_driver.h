/**
 * Real time clock configuration and control - header file
 */

#ifndef RTC_DRIVER_H_
#define RTC_DRIVER_H_

void rtc_init(void);
bool rtc_get_time_16(uint16_t* time_p);


#endif /* RTC_DRIVER_H_ */
