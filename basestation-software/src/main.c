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

void got_packet_data(uint16_t bytes);

void got_packet_data(uint16_t bytes)
{
    uint8_t data[80];

    uint16_t bytes_read = radio_retrieve_data(data, 80);

    printf("\r\nGot some radio data. Count: %d of %d - %d bytes\r\n", data[1], data[2], bytes_read);

    char bughit[] = "Call";
    char temp[] = "Temperature";
    char humid[] = "Humidity";
    char light[] = "Light Level";
    char other[] = "Other";

    for (uint16_t i = 3; i < bytes_read; i += 4)
    {
        char* type;

        switch(data[i + 2] & 0x7F)
        {
            case 0:
                type = bughit;
                break;
            case 1:
                type = temp;
                break;
            case 2:
                type = humid;
                break;
            case 3:
                type = light;
                break;
            default:
                type = other;
        }

        uint32_t timestamp = data[i];
        timestamp |= data[i+1] << 8;
        timestamp |= (data[i+2] & 0x80) << 9;

        printf("%d : %s - %d\r\n", timestamp, type, data[i+3]);
    }
    printf("Data done. \r\n");
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
