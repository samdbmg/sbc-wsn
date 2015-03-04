/**
 * Detection algorithm for use with pin change method
 */

#ifndef PINCHANGE_DETECT_ALGORITHM_H_
#define PINCHANGE_DETECT_ALGORITHM_H_

bool get_detect_active(void);
void initial_detect(void);
void short_timeout(void);
void full_timeout(void);

#endif /* PINCHANGE_DETECT_ALGORITHM_H_ */
