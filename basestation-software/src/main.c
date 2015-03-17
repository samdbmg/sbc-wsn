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

void got_packet_data(uint16_t bytes)
{
    uint8_t data[4] = {0};
    uint8_t send_data[] = "Hey";

    uint16_t bytes_read = radio_retrieve_data(data, 4);

    power_set_minimum(PWR_RADIO, PWR_SLEEP);

    if (data[1] == 'H' && data[2] == 'i')
    {
        radio_send_data(send_data, 3, data[0]);
    }

    radio_receive_activate(false);
    radio_powerstate(false);

    power_set_minimum(PWR_RADIO, PWR_CLOCKSTOP);
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
    radio_powerstate(true);
    radio_send_data("Hi", 2, 0x01);
    radio_receive_activate(true);

    serial_print("Startup done. Sleeping\r\n");

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        power_sleep();
    }
}
