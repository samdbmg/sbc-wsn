/**
 * Speckled Bush-cricket Call Detection Algorithm
 */

// Standard libraries
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control includes */
#include "em_device.h"
#include "em_prs.h"
#include "em_timer.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_pcnt.h"

/* Application-specific includes */
#include "detect_algorithm.h"
#include "misc.h"

/* Functions used only in this file */
static void _detect_timer_config(void);
static void _detect_comparator_config(void);

/* Variable declarations */
static uint8_t call_count;

/**
 * Call the internal functions to start up the SBC detection algorithm
 */
void detect_init(void)
{
    _detect_timer_config();
    _detect_comparator_config();

    call_count = 0;
}


/**
 * Set up a timer for the detection system to use, default to the first state
 */
static void _detect_timer_config(void)
{
    const TIMER_Init_TypeDef timerInit =
    {
        .clkSel = timerClkSelHFPerClk,
        .debugRun = false,
        .dmaClrAct = false,
        .enable = false,
        .fallAction = timerInputActionStop,
        .mode = timerModeUp,
        .oneShot = false,
        .prescale = timerPrescale2,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_Init(TIMER0, &timerInit);

    // Wrap around is about 1kHz
    TIMER_TopSet(TIMER0, get_ticks_from_ms(DETECT_HIGH_UB, DETECT_TIMER_PRESCALER));

    // Enable the overflow interrupt
    TIMER_IntEnable(TIMER0, TIMER_IEN_OF);

    // Power it down until we need it
    CMU_ClockEnable(cmuClock_TIMER0, false);
}

/**
 * Configure the Analog Comparator to detect edges on the input
 */
static void _detect_comparator_config(void)
{

}

