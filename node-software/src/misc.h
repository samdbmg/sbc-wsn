/**
 * Macros and functions useful all over the project but too small to justify
 * their own file go here
 */

#ifndef MISC_H_
#define MISC_H_

#define CORE_CLOCK_FREQ (21000000)
#define TIMER_PRESCALER (2)

// Given a prescaler value (y) and a desired time in ms (x), calculate timer ticks
#define get_ticks_from_ms(x, y)  ((x) * CORE_CLOCK_FREQ)/(1000 * (y))


#endif /* MISC_H_ */
