/**
 * Defines a couple of shared types, enums etc uses by both ends, symlinked
 */

#ifndef RADIO_SHARED_TYPES_H_
#define RADIO_SHARED_TYPES_H_

// Protocol data
#define PKT_TIMESYNC 0x01
#define PKT_REPEAT   0x02
#define PKT_ACK      0x03

#define RADIO_MAX_DATA_LEN 60

/**
 * Types of data we can pick up
 */
typedef enum
{
    DATA_CALL = 0, //!< DATA_CALL
    DATA_TEMP = 1, //!< DATA_TEMP
    DATA_HUMID = 2,//!< DATA_HUMID
    DATA_LIGHT = 3,//!< DATA_LIGHT
    DATA_OTHER = 4 //!< DATA_OTHER
} data_type_t;

/**
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

#endif /* RADIO_SHARED_TYPES_H_ */
