/**
 * Artificial cricket control timers setup header
 *
 */

#ifndef TIMER_CONFIG_H_
#define TIMER_CONFIG_H_

#include <stdbool.h>

/* Function declarations */
void pwm_timer_setup(void);
void markspace_timer_setup(void);
void call_timer_setup(uint32_t call_period_ms, uint32_t female_period_ms);

void call_timer_female(bool enable);

/* Timer speed configuration details */
#define TIMER_BASE_FREQUENCY    72000000

#define PWM_FREQUENCY           40000
#define PWM_TICKS               TIMER_BASE_FREQUENCY/PWM_FREQUENCY
#define PWM_COMPARE_VALUE       PWM_TICKS/2

#define MARKSPACE_FREQUENCY     1000/3 // For 3ms total
#define MARKSPACE_PSC           10     // To fit ticks in 16 bits, prescale is needed
#define MARKSPACE_TICKS         TIMER_BASE_FREQUENCY/(MARKSPACE_FREQUENCY * MARKSPACE_PSC)
#define MARKSPACE_COMPARE_VALUE 2 * MARKSPACE_TICKS/3

#define CALL_TIMER_PSC          TIMER_BASE_FREQUENCY/1000000


#endif /* TIMER_CONFIG_H_ */
