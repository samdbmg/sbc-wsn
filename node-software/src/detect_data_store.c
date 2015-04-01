/**
 * Storage and helper functions for sensor data prior to transmission
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Application-specific headers */
#include "detect_data_store.h"
#include "rtc_driver.h"

data_struct_t data_array[DATA_ARRAY_SIZE];
uint16_t data_array_index = 0;

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

    otherdata &= 0x7F;
    otherdata |= flag ? DATA_FLG_PM : 0x0;

    data_array[data_array_index].time = counter;
    data_array[data_array_index].type = data_type;
    data_array[data_array_index].otherdata = otherdata;

    data_array_index++;
}

/**
 * Return the number of items in the data store
 *
 * @return Number of items
 */
uint16_t store_get_length(void)
{
    return data_array_index;
}

/**
 * Return a pointer to the data array
 * @return Pointer to data array
 */
data_struct_t* store_get_pointer(void)
{
    return data_array;
}

/**
 * Empty the data store
 */
void store_clear(void)
{
    data_array_index = 0;
}
