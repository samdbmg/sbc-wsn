/**
 * Base station software main file
 */

/* Standard libraries */
#include <stdio.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application includes */
#include "radio_control.h"
#include "power_management.h"
#include "serial_interface.h"
#include "printf.h"

#define DEBUG_ENABLE 1

#define NODE_ADDR 0xFF

#define DATA_ARRAY_SIZE 512

static uint8_t incoming_data_array[DATA_ARRAY_SIZE];
static uint16_t incoming_data_pointer = 0;
static uint8_t last_seq_number = 0;
static uint8_t seq_to_repeat[20] = {0};
static uint8_t repeat_index = 0;


void got_packet_data(uint16_t bytes);
void got_complete_packet(void);

void got_packet_data(uint16_t bytes)
{
    // Reset the timeout since we got a new packet
    // TODO

    // Read in enough data to get sequence numbers
    uint8_t seq_data[3];
    uint16_t bytes_read = radio_retrieve_data(seq_data, 3);

    printf("\r\nGot some radio data. Count: %d of %d - %d bytes\r\n", seq_data[1], seq_data[2], bytes);

    // Read rest of data in at correct location
    bytes_read = radio_retrieve_data((incoming_data_array + seq_data[1] * RADIO_MAX_PACKET_LEN), RADIO_MAX_PACKET_LEN);
    incoming_data_pointer += bytes_read;

    if (seq_data[1] != last_seq_number++)
    {
        seq_to_repeat[repeat_index++] = last_seq_number;
    }

    if (seq_data[1] == seq_data[2])
    {
        // Handle a complete packet
        got_complete_packet();
    }
}

void got_complete_packet(void)
{

}

/**
 * Main function.
 *
 * @return Never returns.
 */
void main(void)
{
#ifdef DEBUG_ENABLE
    // Prevent random debugger disconnects
    DBGMCU_Config(DBGMCU_SLEEP | DBGMCU_STOP | DBGMCU_STANDBY, ENABLE);
#endif

    // Activate the serial interface
    serial_init();
    printf("\r\n\r\nStarting up..\r\n\r\n");

    // Configure the radio
    if (!radio_init(0xFF, got_packet_data))
    {
        printf("Radio setup failed\r\nLooping.");
        while (1)
        {

        }
    }
    else
    {
        printf("Radio setup done.\r\n");
    }

    // Set up for receive
    printf("Sending test packet, waiting for RX\r\n");
    radio_powerstate(true);
    uint8_t radio_clock_set_test[] = {0x01, 0x20, 0x00};
    radio_send_data(radio_clock_set_test, sizeof(radio_clock_set_test), 0x01);
    radio_receive_activate(true);

    printf("Startup done. Sleeping\r\n");

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        power_sleep();
    }
}
