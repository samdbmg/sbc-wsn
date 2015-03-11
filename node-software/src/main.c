/**
 * EFM32 Wireless Sensor Node
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control libraries */
#include "em_device.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_gpio.h"

/* Application includes */
#include "misc.h"
#include "detect_algorithm.h"
#include "radio_control.h"
#include "power_management.h"

/**
 * Main function. Initialises peripherals and lets interrupts do the work.
 */
int main(void)
{
    CHIP_Init();

    CMU_HFRCOBandSet(cmuHFRCOBand_21MHz);

    // Low energy module clock supply
    CMU_ClockEnable(cmuClock_CORELE, true);

    // GPIO pins clock supply
    CMU_ClockEnable(cmuClock_GPIO, true);

    // Set up the error LED as an output line
    //GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);

    // Initialise the radio chip
   /* if (!radio_init())
    {
        GPIO_PinOutSet(gpioPortC, 10);
    }

    // Configure the detection algorithm
    detect_init();*/

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        power_sleep();
    }
}

