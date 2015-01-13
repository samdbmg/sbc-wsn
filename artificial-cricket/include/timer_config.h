/**
 * Artificial cricket control timers setup header
 *
 */

#ifndef TIMER_CONFIG_H_
#define TIMER_CONFIG_H_

/* Function declarations */
void pwm_timer_setup(void);
void markspace_timer_setup(void);
void call_timer_setup(uint32_t call_period_ms);


#endif /* TIMER_CONFIG_H_ */
