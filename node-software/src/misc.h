/**
 * Macros and functions useful all over the project but too small to justify
 * their own file go here
 */

#ifndef MISC_H_
#define MISC_H_

#define CORE_CLOCK_FREQ (21000000)
#define TIMER_PRESCALER (2)

// Given a prescaler value (y) and a desired time in ms (x), calculate timer ticks
#define get_ticks_from_ms(x, y)  ((uint64_t)(x) * CORE_CLOCK_FREQ)/(1000 * (y))

void misc_delay(uint16_t ms, bool block);
bool misc_delay_active(void);
void misc_delay_init(void);


#endif /* MISC_H_ */
