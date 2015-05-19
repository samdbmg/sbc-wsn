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
#include "radio_protocol.h"
#include "status_leds.h"
#include "printf.h"

/* Functions used only in this file */
static void clocks_init(void);

/**
 * Main function. Initialises peripherals and lets interrupts do the work.
 */
int main(void)
{
    CHIP_Init();

    clocks_init();

    // Configure interrupts to show LED status, light yellow to show
    // startup running
    status_init();
    status_led_set(STATUS_YELLOW, true);

    // Configure external sensor interface and debugging
    ext_init();

    // Announce startup on debug interface
    printf("\r\n\r\n\r\n");
    printf("==============\r\n");
    printf("Starting up...\r\n");

    // Configure the delay function
    misc_delay_init();

    // Initialise the radio chip
    if (!radio_init(proto_read_address(), proto_incoming_packet))
    {
    	status_led_set(STATUS_RED, true);
    	status_illuminate(true);

    	printf("Radio setup fails...\r\n");

    	while (true)
    	{

    	}
    }

    printf("Radio ready\r\n");

    // Configure the detection algorithm
    detect_init();

    printf("Detection ready\r\n");

    // Configure sensors
    sensors_init();

    printf("Sensors ready\r\n");

    // Start the RTC (it will be set when the radio protocol kicks in)
    rtc_init();
    proto_run();

    radio_powerstate(true);

    // Kill LED, set green (all good), startup complete
    printf("RTC ready.\r\nStartup complete\r\n\r\n");
    status_led_set(STATUS_YELLOW, false);
    status_led_set(STATUS_GREEN, true);

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        proto_run();
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
    CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_LFXOBOOST_MASK) | CMU_CTRL_LFXOBOOST_70PCENT;

    // Assign the LFRCO to LF clock A and B domains (RTC, LEUART)
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFRCO);

    // GPIO pins clock supply
    CMU_ClockEnable(cmuClock_GPIO, true);
}

