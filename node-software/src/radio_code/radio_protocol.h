/**
 * Implements data send and receive functions - header file
 */

#ifndef RADIO_PROTOCOL_H_
#define RADIO_PROTOCOL_H_

typedef enum {PROTO_SETUP, PROTO_IDLE, PROTO_SEND, PROTO_WAITACK} proto_radio_state_t;

#define BASE_ADDR 0xFF
#define NODE_ADDR 0x01

void proto_init(void);
void proto_incoming_packet(uint16_t bytes);

void proto_run(void);

void proto_triggerupload(void);

#endif /* RADIO_PROTOCOL_H_ */
