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

/* Application includes */
#include "misc.h"
#include "detect_algorithm.h"
#include "radio_control.h"

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

    // Initialise the radio chip
    bool ret = radio_init();

    // Configure the detection algorithm
    //detect_init();

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        //TODO
    }
}

