/**
 * Protocol control for RFM69W radio module - header file
 * This code should provide high-level control of the module without making any
 * processor-specific calls so should be fairly portable
 */

#ifndef RADIO_CONTROL_H_
#define RADIO_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

bool radio_init(void);

// Internal functions for sending and receiving data - exposed for convienience
bool _radio_send_data(char* data_p, uint16_t length);
uint16_t _radio_receive_data(char* data_p, uint16_t length);




#endif /* RADIO_CONTROL_H_ */
