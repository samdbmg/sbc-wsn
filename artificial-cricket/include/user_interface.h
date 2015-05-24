/**
 * Function to activate the user switch and interrupts to use it - header file
 *
 */

#ifndef USER_SWITCH_H_
#define USER_SWITCH_H_

#define ZERO_ABOVE_THRESHOLD 3500

void user_switch_init(void);
void ext_switch_init(void);
void adc_init(void);

#endif /* USER_SWITCH_H_ */
