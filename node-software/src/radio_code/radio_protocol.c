/**
 * Implements data send and receive functions
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */


/* Application-specific headers */
#include "radio_protocol.h"
#include "radio_control.h"
#include "radio_shared_types.h"
#include "power_management.h"
#include "i2c_sensors.h"
#include "detect_data_store.h"
#include "misc.h"
#include "rtc_driver.h"

// Set to zero to keep the radio awake when idle
#define RADIO_SLEEP_IDLE 0

#define RADIO_TIMEOUT 3000

// Protocol state store
static proto_radio_state_t proto_state;

// End point in datastore, used to clear store once all data received
static uint16_t datastore_end;

// Assemble some storage for the packet data array
static uint8_t packet_data[RADIO_MAX_PACKET_LEN];

// Functions used only in this file
static void _proto_endcleanup(void);
static void _proto_uploaddata(void);

/**
 * Initialise protocol and start setup process
 */
void proto_init(void)
{
    proto_state = PROTO_SETUP;
}

/**
 * Handler for incoming data packets - identifies packet type and takes action
 *
 * @param bytes Number of bytes received in packet
 */
void proto_incoming_packet(uint16_t bytes)
{
    uint8_t data[20];

    // Read data from ringbuffer
    radio_retrieve_data(data, 20);

    // Process the packet based on a type header
    switch (data[1])
    {
        case PKT_TIMESYNC:
        {
            // Next two bytes are an RTC update, update the RTC
            uint16_t timestamp = data[2];
            timestamp |= data[3] << 8;

            rtc_set_time(timestamp);
            break;
        }
        case PKT_REPEAT:
        {
            // Reset the ACK timer
            misc_delay(RADIO_TIMEOUT, false);

            // Third byte should be total sequence size
            packet_data[0] = data[2];

            // Fourth should be sequence to repeat
            packet_data[1] = data[3];

            uint8_t packet_len;

            if (data[2] == data[3])
            {
                // Last packet was missed, send a short one
                packet_len = store_get_size() -
                        ((RADIO_MAX_PACKET_LEN - 2)/sizeof(data_struct_t)) * data[3];
            }
            else
            {
                packet_len = (RADIO_MAX_PACKET_LEN - 2)/sizeof(data_struct_t);
            }

            store_get_data(&(packet_data[2]), packet_len,
                    ((RADIO_MAX_PACKET_LEN - 2)/sizeof(data_struct_t)) * data[3]);

            radio_send_data(packet_data,
                    (packet_len * sizeof(data_struct_t)) + 2, BASE_ADDR);

            break;
        }
        case PKT_ACK:
        {
            // Finish up
            _proto_endcleanup();

            break;
        }
        default:
            // Ignore an unknown packet
            break;
    }

}

/**
 * Operate protocol state machine
 */
void proto_run(void)
{
    switch (proto_state)
    {
        case PROTO_SEND:
        {
            // Send the data
            _proto_uploaddata();

            // Enable receive mode and wait for acknowledge
            radio_receive_activate(true);
            proto_state = PROTO_WAITACK;

            misc_delay(RADIO_TIMEOUT, false);
            break;
        }
        case PROTO_WAITACK:
        {
            if (!misc_delay_active())
            {
                // Timer's ended, let's assume we didn't get an ACK,
                // clear store and go back to sleep
                _proto_endcleanup();
            }
            // If timer is still active, spurious wake from something else,
            // ignore.
            break;
        }
        case PROTO_SETUP:
        {
            // TODO Work out how to make it keep sending pulses (RTC trick?)

            // For now go straight to idle
            proto_state = PROTO_IDLE;

            break;
        }
        case PROTO_IDLE:
        default:
            // Nothing to do
            break;
    }

}

/**
 * Used by RTC timeout to trigger an upload to start
 */
void proto_triggerupload(void)
{
    proto_state = PROTO_SEND;
    // This will exit the interrupt handler into proto_run and stuff will happen
}

/**
 * Cleanup to go back into idle mode
 */
static void _proto_endcleanup(void)
{
    store_clear(datastore_end);

#if RADIO_SLEEP_IDLE
    radio_powerstate(false);
#endif

    proto_state = PROTO_IDLE;
}

/**
 * Actually package up data for upload and run it
 */
static void _proto_uploaddata(void)
{
    // Measure environment
    store_other(DATA_TEMP, (uint8_t)sensors_read(SENS_TEMP));
    store_other(DATA_HUMID, (uint8_t)sensors_read(SENS_HUMID));
    store_other(DATA_LIGHT, (uint8_t)sensors_read(SENS_LIGHT));

    // Compute how many packets need to be sent (bytes in store by bytes in a
    // packet after overheads)
    uint8_t packet_count = ((store_get_size() * sizeof(data_struct_t)) /
            (RADIO_MAX_PACKET_LEN - 2)) + 1;

    datastore_end = store_get_write_position();
    uint8_t seq_number = 1;
    uint8_t data_pointer = 0;

    packet_data[0] = packet_count;

    // Turn the radio on
    radio_powerstate(true);

    // Compose and send a string of full packets
    for (uint8_t i = 0; i < packet_count - 1; i++)
    {
        packet_data[1] = seq_number++;

        store_get_data(&(packet_data[2]),
                (RADIO_MAX_PACKET_LEN - 2)/sizeof(data_struct_t), data_pointer);

        radio_send_data(packet_data, RADIO_MAX_PACKET_LEN, BASE_ADDR);

        data_pointer += RADIO_MAX_PACKET_LEN;
    }

    // Final packet may be smaller
    packet_data[1] = seq_number;
    uint8_t packet_len = store_get_size();
    packet_len -= ((seq_number - 1) * ((RADIO_MAX_PACKET_LEN - 2)/sizeof(data_struct_t)));

    store_get_data(&(packet_data[2]), packet_len, data_pointer);

    radio_send_data(packet_data,
            (packet_len * sizeof(data_struct_t)) + 2, BASE_ADDR);
}
