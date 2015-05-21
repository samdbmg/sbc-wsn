/**
 * Speckled Bush-cricket Call Detection Algorithm - header file
 */
#ifndef DETECT_ALGORITHM_H_
#define DETECT_ALGORITHM_H_

/* Enable algorithm debug mode (uses more memory and time) */
//#define DETECT_DEBUG_ON        1

/* Algorithm configuration */
#define DETECT_PSC             16

// Upper and lower bound of detect high and low states (found with ((t * 21MHz)/(1000 * PSC)) where PSC is above
#define DETECT_HIGH_UB         3938 // 3ms
#define DETECT_HIGH_LB         984 // 0.75ms
#define DETECT_LOW_UB          3938 // 3ms
#define DETECT_LOW_LB          984 // 0.75ms
#define DETECT_WAIT_F_UB	   45938 // 35ms
#define DETECT_WAIT_F_LB       32813 // 25ms
#define DETECT_LOW_F_UB		   5250 // 4ms

#define DETECT_MINCOUNT        5
#define DETECT_MAXCOUNT        8

#define DETECT_TRANSIENTTH     5

// State declarations
#define DETECT_IDLE 0
#define DETECT_HIGH 1
#define DETECT_LOW  2
#define DETECT_HIGH_F 3
#define DETECT_LOW_F 4
#define DETECT_WAIT_F 5


void detect_init(void);

#endif /* DETECT_ALGORITHM_H_ */
