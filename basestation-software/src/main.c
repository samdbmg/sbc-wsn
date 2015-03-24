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

#define DEBUG_ENABLE 1

#define NODE_ADDR 0xFF

void got_packet_data(uint16_t bytes);
void output_byte_as_text(uint8_t byte);

void output_byte_as_text(uint8_t byte)
{
    uint8_t print[] = "0xXX";
    print[2] = (byte & 0xF0) >> 4;
    print[2] = print[2] <= 9 ? '0' + print[2] : 'A' - 10 + print[2];

    print[3] = byte & 0x0F;
    print[3] = print[3] <= 9 ? '0' + print[3] : 'A' - 10 + print[3];

    serial_print((char *)print);
}

void got_packet_data(uint16_t bytes)
{
    uint8_t data[80];

    uint16_t bytes_read = radio_retrieve_data(data, 80);

    serial_print("\r\nGot some radio data. Count: ");
    output_byte_as_text(data[1]);
    serial_print(" of ");
    output_byte_as_text(data[2]);
    serial_print("\r\n");

    for (uint16_t i = 3; i < bytes_read; i += 4)
    {
        output_byte_as_text(data[i]);
        serial_print(", ");
        output_byte_as_text(data[i + 1]);
        serial_print(", ");
        output_byte_as_text(data[i + 2]);
        serial_print(", ");
        output_byte_as_text(data[i + 3]);
        serial_print("\r\n");
    }
    serial_print("Data done. \r\n");
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
    serial_print("\r\n\r\nStarting up..\r\n\r\n");

    // Configure the radio
    if (!radio_init(0xFF, got_packet_data))
    {
        serial_print("Radio setup failed\r\nLooping.");
        while (1)
        {

        }
    }
    else
    {
        serial_print("Radio setup done.\r\n");
    }

    // Set up for receive
    serial_print("Sending test packet, waiting for RX\r\n");
    radio_powerstate(true);
    radio_send_data((uint8_t*)"Hi", 2, 0x01);
    radio_receive_activate(true);

    serial_print("Startup done. Sleeping\r\n");

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        power_sleep();
    }
}
