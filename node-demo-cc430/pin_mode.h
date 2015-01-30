/**
 * Hardware configuration for rising edge detect algorithm - header
 */

#ifndef PIN_MODE_H_
#define PIN_MODE_H_

#define CORE_CLOCK_FREQ 12000000
#define TIMER_PRESCALER 4

#define get_ticks_from_ms(x)  (x * CORE_CLOCK_FREQ)/(1000 * TIMER_PRESCALER)


void pin_mode_init(void);
void sleep_handler(void);

#endif /* PIN_MODE_H_ */
