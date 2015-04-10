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
#include "radio_protocol.h"
#include "printf.h"
#include "gsm_modem.h"
#include "base_misc.h"

#define DEBUG_ENABLE 1

#define NODE_ADDR 0xFF

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
    init_printf(NULL, serial_printf_out);
    printf("\r\n\r\nStarting up..\r\n\r\n");

    misc_delay_init();

    modem_setup();
    modem_connection(true);
    modem_gettime();


    // Configure the radio
    if (!radio_init(0xFF, proto_incoming_packet))
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

    proto_init();

    // Set up for receive
    printf("Sending test packet, waiting for RX\r\n");
    radio_powerstate(true);
    uint8_t radio_clock_set_test[] = {0x01, 0x20, 0x00};
    radio_send_data(radio_clock_set_test, sizeof(radio_clock_set_test), 0x01);
    proto_start_rec();

    printf("Startup done. Sleeping\r\n");

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        proto_run();
        power_sleep();
    }
}
