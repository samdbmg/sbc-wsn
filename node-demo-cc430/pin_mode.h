/**
 * Hardware configuration for rising edge detect algorithm - header
 */

#ifndef PIN_MODE_H_
#define PIN_MODE_H_

#define CORE_CLOCK_FREQ 2000000
#define TIMER_PRESCALER 2

#define get_ticks_from_ms(x)  (x * CORE_CLOCK_FREQ)/(1000 * TIMER_PRESCALER)


void pin_mode_init(void);


#endif /* PIN_MODE_H_ */
