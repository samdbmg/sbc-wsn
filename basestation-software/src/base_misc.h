/**
 * Miscellaneous helper functions that don't live anywhere else - header file
 */


#ifndef BASE_MISC_H_
#define BASE_MISC_H_

#include <stdint.h>
#include <stdbool.h>

#define PERIPH_CLOCK_FREQ (32000000)

// Given a prescaler value (y) and a desired time in ms (x), calculate timer ticks
#define get_ticks_from_ms(x, y)  ((uint64_t)(x) * PERIPH_CLOCK_FREQ)/(1000 * (y))

// Typedef to support ring buffer functions
typedef struct
{
    uint8_t* data_pointer;
    uint8_t* write_pointer;
    uint8_t* read_pointer;
    uint16_t length;
} misc_ringbuf_t;

// Delay timer
void misc_delay_init(void);
void misc_delay(uint32_t ms, bool block);
bool misc_delay_active(void);

// Ringbuffers are commonly used, so this saves some duplication
void misc_ringbuffer_write(misc_ringbuf_t *buffer, uint8_t* pdata, uint16_t bytes);
uint16_t misc_ringbuffer_read(misc_ringbuf_t *buffer, uint8_t* pdata, uint16_t bytes);
void misc_ringbuffer_clear(misc_ringbuf_t *buffer);

#endif /* BASE_MISC_H_ */
