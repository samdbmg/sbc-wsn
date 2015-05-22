/**
 * Storage and helper functions for sensor data prior to transmission - header
 * file
 */

#ifndef DETECT_DATA_STORE_H_
#define DETECT_DATA_STORE_H_

#include "radio_shared_types.h"

// Detect flags
#define DATA_FLG_FEM 0x80 // Marks a probable female call was heard

void store_call(bool female, uint8_t clicks);
void store_other(data_type_t data_type, uint8_t data);

uint16_t store_get_size(void);
uint16_t store_get_write_position(void);
void store_get_data(uint8_t *data_p, uint16_t length, uint16_t skip);
void store_clear(uint16_t write_position);

#endif /* DETECT_DATA_STORE_H_ */
