/**
 * Speckled Bush-cricket Call Detection Algorithm - header
 */
#ifndef DETECT_ALGORITHM_H_
#define DETECT_ALGORITHM_H_

/* Algorithm configuration */
#define DETECT_TIMER_PRESCALER 2

// Upper and lower bound of detect high and low states
#define DETECT_HIGH_UB         1.3
#define DETECT_HIGH_LB         0.7
#define DETECT_LOW_UB          2.3
#define DETECT_LOW_LB          1.7

// State declarations
#define DETECT_IDLE 0
#define DETECT_HIGH 1
#define DETECT_LOW  2


void detect_init(void);



#endif /* DETECT_ALGORITHM_H_ */
