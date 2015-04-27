/**
 * Implements data send and receive functions - header file
 */

#ifndef RADIO_PROTOCOL_H_
#define RADIO_PROTOCOL_H_

typedef enum {PROTO_SETUP, PROTO_IDLE, PROTO_SEND, PROTO_WAITACK} proto_radio_state_t;

#define BASE_ADDR 0xFF

void proto_init(void);
void proto_incoming_packet(uint16_t bytes);
uint8_t proto_read_address(void);

void proto_run(void);

void proto_triggerupload(void);

#endif /* RADIO_PROTOCOL_H_ */
