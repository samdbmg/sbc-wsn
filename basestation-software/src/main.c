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
#include "rtc_driver.h"

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

    rtc_init();
    proto_init();

    printf("Startup done. Sleeping\r\n");

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        proto_run();
        power_sleep();
    }
}
