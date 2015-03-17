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
    // Configure the radio
    radio_init(0xFF, got_packet_data);

    // Set up for receive
    radio_powerstate(true);
    radio_receive_activate(true);

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        power_sleep();
    }
}
