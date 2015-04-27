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

/* Functions used only in this file */
static void clocks_init(void);

/**
 * Main function. Initialises peripherals and lets interrupts do the work.
 */
int main(void)
{
    CHIP_Init();

    clocks_init();

    // Configure interrupts to show LED status, light green to show
    // startup running
    status_init();
    status_led_set(STATUS_YELLOW, true);

    // Configure the delay function
    misc_delay_init();

    // Initialise the radio chip
    if (!radio_init(NODE_ADDR, proto_incoming_packet))
    {
    	status_led_set(STATUS_RED, true);
    	status_illuminate(true);

    	while (true)
    	{

    	}
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
    proto_run();
    //proto_triggerupload();

    // Kill LED, startup complete
    status_led_set(STATUS_YELLOW, false);

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        proto_run();
        power_sleep();
        //proto_triggerupload();
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

