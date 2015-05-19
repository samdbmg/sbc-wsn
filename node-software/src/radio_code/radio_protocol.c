/**
 * Implements data send and receive functions
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_gpio.h"

/* Application-specific headers */
#include "radio_protocol.h"
#include "radio_control.h"
#include "radio_shared_types.h"
#include "radio_schedule_settings.h"
#include "power_management.h"
#include "i2c_sensors.h"
#include "detect_data_store.h"
#include "misc.h"
#include "rtc_driver.h"
#include "status_leds.h"
#include "printf.h"

// Set to zero to keep the radio awake when idle
#define RADIO_SLEEP_IDLE 1

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

            rtc_set_time(timestamp, data[4] & 0x01);
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

            printf("Repeat request for %d of %d\r\n", data[3], data[2]);

            uint8_t packet_len;

            if (data[2] == data[3])
            {
                // Last packet was missed, send a short one
                packet_len = store_get_size() - RADIO_MAX_DATA_LEN * (data[3] - 1);
            }
            else
            {
                packet_len = RADIO_MAX_DATA_LEN;
            }

            store_get_data(&(packet_data[2]), packet_len,
                    RADIO_MAX_DATA_LEN * (data[3] - 1));

            // Brief delay to allow far end to flip back to receive
            misc_delay(200, true);

            radio_send_data(packet_data, packet_len, BASE_ADDR);

            break;
        }
        case PKT_ACK:
        {
            // Finish up
            _proto_endcleanup();

            printf("Got ACK\r\n");

            break;
        }
        case PKT_BEACONACK:
        {
        	// Packet should be [time(16)],[period(16)],[nextwake(16)],[options(8)]
        	printf("Got BEACONACK...");

        	uint32_t time_now = data[0] << 8 | data[1];
        	rtc_set_time(time_now, (data[6] & 0x01));

        	uint32_t period = data[2] << 8 | data[3];
        	period |= (data[6] & 0x02) << 15;

        	uint32_t next_wake = data[2] << 8 | data[3];
        	next_wake |= (data[6] & 0x02) << 14;

        	rtc_set_schedule(period, next_wake);

        	proto_state = PROTO_IDLE;
        	_proto_endcleanup();

        	printf("setup complete\r\n");

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
            radio_receive_activate(true);

            // Send the data
            _proto_uploaddata();

            // Wait for acknowledge
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

                printf("No ACK, timed out\r\n");
            }
            // If timer is still active, spurious wake from something else,
            // ignore.
            break;
        }
        case PROTO_SETUP:
        {
        	proto_state = PROTO_IDLE;

        	// Set the RTC up to send beacon frames
        	rtc_set_time(0, 0);
        	rtc_set_schedule(RSCHED_BEACONPERIOD, 1);

            break;
        }
        case PROTO_BEACON:
        {
        	// Prepare a beacon frame
        	packet_data[0] = 1;
        	packet_data[1] = 1;
        	packet_data[2] = PKT_BEACON;

        	// Send the beacon frame
        	radio_send_data(packet_data, 3, BASE_ADDR);

        	// Wait for the response
        	proto_state = PROTO_WAITBEACON;
            misc_delay(RADIO_TIMEOUT, false);

        	break;
        }
        case PROTO_WAITBEACON:
        {
            if (!misc_delay_active())
            {
                // Timer's ended, let's assume we didn't get an ACK,
                // go back to sleep

#if RADIO_SLEEP_IDLE
            	radio_powerstate(false);
#endif

                proto_state = PROTO_SETUP;
            	status_led_set(STATUS_GREEN, true);
            }
            // If timer is still active, spurious wake from something else,
            // ignore.
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
	status_led_set(STATUS_GREEN, false);

	if (proto_state == PROTO_SETUP)
	{
		// In setup mode we prepare to send a beacon frame
		proto_state = PROTO_BEACON;
	}
	else
	{
		proto_state = PROTO_SEND;
	}
    // This will exit the interrupt handler into proto_run and stuff will happen
}

/*
 * Read the node address from the DIP switches
 *
 * @return Address set on switches
 */
uint8_t proto_read_address(void)
{
	// Enable all the address pins
	GPIO_PinModeSet(gpioPortC, 10, gpioModeInputPull, 1);
	GPIO_PinModeSet(gpioPortC, 11, gpioModeInputPull, 1);
	GPIO_PinModeSet(gpioPortC, 13, gpioModeInputPull, 1);
	GPIO_PinModeSet(gpioPortC, 14, gpioModeInputPull, 1);

	GPIO_PinModeSet(gpioPortD, 6, gpioModeInputPull, 1);

	// Capture the low end and shift as needed
	uint8_t addr_bits = (GPIO_PortInGet(gpioPortC) & 0x0F00) >> 8;
	addr_bits |= (GPIO_PortInGet(gpioPortC) & 0x6000) >> 9; // Adjust for no bit 12

	// Capture top bit
	addr_bits |= GPIO_PinInGet(gpioPortD, 6);

	// Disable all the address pins
	GPIO_PinModeSet(gpioPortC, 10, gpioModeDisabled, 0);
	GPIO_PinModeSet(gpioPortC, 11, gpioModeDisabled, 0);
	GPIO_PinModeSet(gpioPortC, 13, gpioModeDisabled, 0);
	GPIO_PinModeSet(gpioPortC, 14, gpioModeDisabled, 0);

	GPIO_PinModeSet(gpioPortD, 6, gpioModeDisabled, 0);

	// We flip the address because switch "on" sets to low
	return ~addr_bits;
}

/**
 * Cleanup to go back into idle mode
 */
static void _proto_endcleanup(void)
{
    store_clear(datastore_end);
    //radio_receive_activate(false);

#if RADIO_SLEEP_IDLE
    radio_powerstate(false);
#endif

    proto_state = PROTO_IDLE;
	status_led_set(STATUS_GREEN, true);
}

/**
 * Actually package up data for upload and run it
 */
static void _proto_uploaddata(void)
{
	printf("Beginning data upload...");

    // Measure environment
    store_other(DATA_TEMP, (uint8_t)sensors_read(SENS_TEMP));
    store_other(DATA_HUMID, (uint8_t)sensors_read(SENS_HUMID));
    store_other(DATA_LIGHT, (uint8_t)sensors_read(SENS_LIGHT));

    // Now go generate enough data to keep us busy
    for (uint8_t i = 0; i < 50; i++)
    {
        store_other(DATA_OTHER, i);
    }

    // Compute how many packets need to be sent (bytes in store by bytes in a
    // packet after overheads)
    uint8_t packet_count = (store_get_size() / RADIO_MAX_DATA_LEN) + 1;

    datastore_end = store_get_write_position();
    uint8_t seq_number = 1;
    uint8_t data_pointer = 0;

    packet_data[0] = packet_count;

    // Turn the radio on and enable receive for acking
    radio_powerstate(true);
    radio_receive_activate(true);

    // Compose and send a string of full packets
    for (uint8_t i = 0; i < packet_count - 1; i++)
    {
        packet_data[1] = seq_number++;

        store_get_data(&(packet_data[2]), RADIO_MAX_DATA_LEN, data_pointer);

        radio_send_data(packet_data, RADIO_MAX_DATA_LEN + 2, BASE_ADDR);

        data_pointer += RADIO_MAX_DATA_LEN;
    }

    // Final packet may be smaller
    packet_data[1] = seq_number;
    uint8_t packet_len = store_get_size();
    packet_len -= (seq_number - 1) * RADIO_MAX_DATA_LEN;

    store_get_data(&(packet_data[2]), packet_len, data_pointer);

    radio_send_data(packet_data, packet_len + 2, BASE_ADDR);

    printf("done\r\n");
}
