/**
 * EFM32 Wireless Sensor Node
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_gpio.h"

/* Application-specific headers */
#include "main.h"
#include "misc.h"
#include "detect_algorithm.h"
#include "radio_control.h"
#include "power_management.h"
#include "i2c_sensors.h"
#include "rtc_driver.h"
#include "detect_data_store.h"
#include "ext_sensor.h"

#define NODE_ADDR 0x01
#define BASE_ADDR 0xFF

#define PKT_TIMESYNC 0x01
#define PKT_REPEAT   0x02
#define PKT_ACK      0x03

#define RADIO_TIMEOUT 1000

// End point in datastore, used to clear store once all data received
static uint16_t datastore_end;

// Flag to mark that we haven't had an ACK from the base and should keep
// waiting for timeout
static bool ack_wait = false;

// Assemble some storage for the packet data array
static uint8_t packet_data[RADIO_MAX_PACKET_LEN];

/* Functions used only in this file */
static void clocks_init(void);

void got_packet_data(uint16_t bytes)
{
    uint8_t data[20];

    // Read data from ringbuffer
    uint16_t read_bytes = radio_retrieve_data(data, sizeof(data));

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
            // End the timer loop
            ack_wait = false;

            break;
        }
        default:
            // Ignore an unknown packet
            break;
    }
}

/**
 * Called every hour (ish) to upload fresh data to the base node
 */
void main_rtc_timeout_upload_data(void)
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

    // Enable receive mode and wait for acknowledge
    radio_receive_activate(true);
    ack_wait = true;

    misc_delay(RADIO_TIMEOUT, false);

    while (ack_wait && misc_delay_active())
    {
        power_sleep();
    }

    // Empty the datastore
    store_clear(datastore_end);

    // Sleep the radio until the next wake
    radio_powerstate(false);

}

/**
 * Main function. Initialises peripherals and lets interrupts do the work.
 */
int main(void)
{
    CHIP_Init();

    clocks_init();

    // Set up the error LED as an output line
    GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);

    // Configure the delay function
    misc_delay_init();

    // Initialise the radio chip
    if (!radio_init(NODE_ADDR, got_packet_data))
    {
        GPIO_PinOutSet(gpioPortC, 10);
    }

    // Configure the detection algorithm
    detect_init();

    // Configure sensors
    sensors_init();

    // Configure external sensor interface
    ext_init();

    // Prep radio for receive
    radio_powerstate(true);
    radio_receive_activate(true);

    // Start the RTC (it will be set when the radio protocol kicks in)
    rtc_init();
    //main_rtc_timeout_upload_data();

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        power_sleep();
    }
}

static void clocks_init(void)
{
    CMU_HFRCOBandSet(cmuHFRCOBand_21MHz);

    // Low energy module clock supply
    CMU_ClockEnable(cmuClock_CORELE, true);

    // Start the LFRCO (Low Freq RC Oscillator) and wait for it to stabilise
    // The ULFRCO uses less power, but is much less accurate!
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);

    // Assign the LFRCO to LF clock A and B domains (RTC, LEUART)
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFRCO);

    // GPIO pins clock supply
    CMU_ClockEnable(cmuClock_GPIO, true);
}

