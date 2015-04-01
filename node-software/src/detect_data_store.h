/**
 * Storage and helper functions for sensor data prior to transmission - header
 * file
 */

#ifndef DETECT_DATA_STORE_H_
#define DETECT_DATA_STORE_H_

typedef enum
{
    DATA_CALL = 0,
    DATA_TEMP = 1,
    DATA_HUMID = 2,
    DATA_LIGHT = 3,
    DATA_OTHER = 4
} data_type_t;

/*
 * Data storage type. Note that 17 bits are required to store a timestamp as
 * an offset from midnight in seconds, so the MSB of type is used as well
 */
typedef struct
{
    uint16_t time;
    uint8_t type;
    uint8_t otherdata;
} data_struct_t;

#define DATA_ARRAY_SIZE 512

// Detect flags
#define DATA_FLG_FEM (1 << 1) // Marks a probable female call was heard

void store_call(bool female);
void store_other(data_type_t data_type, uint8_t data);

uint16_t store_get_length(void);
data_struct_t* store_get_pointer(void);
void store_clear(void);

#endif /* DETECT_DATA_STORE_H_ */
