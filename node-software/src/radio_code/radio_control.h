/**
 * Protocol control for RFM69W radio module - header file
 * This code should provide high-level control of the module without making any
 * processor-specific calls so should be fairly portable
 */

#ifndef RADIO_CONTROL_H_
#define RADIO_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

#define RADIO_MAX_PACKET_LEN  61

bool radio_init(void);

// Internal functions for sending and receiving data - exposed for convienience
bool _radio_send_data(char* data_p, uint16_t length, uint8_t dest_addr);
uint16_t _radio_retrieve_data(char* data_p, uint16_t length);
void _radio_activate_receiver(bool activate);

// Functions for low-level interrupt routines to make callbacks
void _radio_payload_ready(void);


#endif /* RADIO_CONTROL_H_ */
