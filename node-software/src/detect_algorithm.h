/**
 * Speckled Bush-cricket Call Detection Algorithm - header file
 */
#ifndef DETECT_ALGORITHM_H_
#define DETECT_ALGORITHM_H_

/* Enable algorithm debug mode (uses more memory and time) */
//#define DETECT_DEBUG_ON        1

/* Algorithm configuration */
#define DETECT_PSC             2

// Upper and lower bound of detect high and low states
#define DETECT_HIGH_UB         2.5
#define DETECT_HIGH_LB         0.9
#define DETECT_LOW_UB          2.5
#define DETECT_LOW_LB          0.9

#define DETECT_MINCOUNT        5
#define DETECT_MAXCOUNT        8

#define DETECT_TRANSIENTTH     5

// State declarations
#define DETECT_IDLE 0
#define DETECT_HIGH 1
#define DETECT_LOW  2


void detect_init(void);

#endif /* DETECT_ALGORITHM_H_ */
