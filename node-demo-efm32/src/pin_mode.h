/**
 * EFM32 Node demo for pin change mode - header file
 */

#ifndef PIN_MODE_H_
#define PIN_MODE_H_

void pin_mode_init(void);

void rtc_handler(void);
void sleep_handler(void);

void pin_timer_init(void);
void pinchange_init(void);

#endif /* PIN_MODE_H_ */
