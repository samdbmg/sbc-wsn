/**
 * Implements access control and packet receive functions - header file
 */

#ifndef RADIO_CODE_RADIO_PROTOCOL_H_
#define RADIO_CODE_RADIO_PROTOCOL_H_

typedef enum {PROTO_IDLE, PROTO_AWAKE, PROTO_RECV, PROTO_ARQ, PROTO_REPEATING} proto_radio_state_t;

void proto_init(void);
void proto_incoming_packet(uint16_t bytes);
void proto_start_rec(void);

void proto_run(void);

#endif /* RADIO_CODE_RADIO_PROTOCOL_H_ */
