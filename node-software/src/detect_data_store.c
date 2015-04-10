/**
 * Storage and helper functions for sensor data prior to transmission
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Application-specific headers */
#include "detect_data_store.h"
#include "rtc_driver.h"

data_struct_t data_array[DATA_ARRAY_SIZE];
uint16_t data_write_index = 0;
uint16_t data_read_index = 0;

/**
 * Store a cricket call at the current time
 * @param female True if a female call was suspected
 */
void store_call(bool female)
{
    store_other(DATA_CALL, (female ? DATA_FLG_FEM : 0x0));
}

/**
 * Store a data point at the current time
 * @param data_type One of the DATA_x enum types of data
 * @param otherdata 7 bits of data (sensor hit etc) to be stored - MSB used as
 *                  PM time flag
 */
void store_other(data_type_t data_type, uint8_t otherdata)
{
    uint16_t counter;
    bool flag = rtc_get_time_16(&counter);

    data_type &= 0x7F;
    data_type |= flag ? 0x80 : 0x0;

    data_array[data_write_index].time = counter;
    data_array[data_write_index].type = data_type;
    data_array[data_write_index].otherdata = otherdata;

    data_write_index++;

    if (data_write_index >= DATA_ARRAY_SIZE)
    {
        data_write_index = 0;
    }

    if (data_write_index == data_read_index)
    {
        data_read_index++;

        if (data_read_index >= DATA_ARRAY_SIZE)
        {
            data_read_index = 0;
        }
    }
}


/**
 * Retrieve the total number of items held in the data store
 * @return Number of bytes
 */
uint16_t store_get_size(void)
{
    if (data_write_index >= data_read_index)
    {
        return (data_write_index - data_read_index) * sizeof(data_struct_t);
    }
    else
    {
        return (DATA_ARRAY_SIZE - (data_read_index - data_write_index)) *
                sizeof(data_struct_t);
    }
}

/**
 * Retrieve the current position of the write cursor, used in a later call to
 * store_clear()
 * @return Position of write pointer
 */
uint16_t store_get_write_position(void)
{
    return data_write_index;
}

/**
 * Retrieve a block of data from the data store
 *
 * @param data_p Pointer to write the data into
 * @param length Number of bytes to retrieve, must be less than total available
 * @param skip   Number of bytes to skip ahead by
 */
void store_get_data(uint8_t *data_p, uint16_t length, uint16_t skip)
{
    int16_t internal_len = length;

    uint16_t index = data_read_index + (skip / sizeof(data_struct_t));

    while (internal_len > 0)
    {
        memcpy(data_p, (data_array + index++), sizeof(data_struct_t));
        data_p += sizeof(data_struct_t);

        if (index >= DATA_ARRAY_SIZE)
        {
            index = 0;
        }
        internal_len -= sizeof(data_struct_t);
    }
}

/**
 * Empty the data store
 *
 * @param write_position Write pointer from a previous call to
 *                       store_get_write_position()
 */
void store_clear(uint16_t write_position)
{
    data_read_index = write_position;
}
