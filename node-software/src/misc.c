/**
 * Macros and functions useful all over the project but too small to justify
 * their own file go here
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_timer.h"

/* Application-specific headers */
#include "misc.h"
#include "power_management.h"

static volatile bool _delay_active = false;

/**
 * Do all the run-once config for the delay timer
 */
void misc_delay_init(void)
{
    // Run clock to the timer
    CMU_ClockEnable(cmuClock_TIMER1, true);

    const TIMER_Init_TypeDef timerInit =
    {
        .clkSel = timerClkSelHFPerClk,
        .debugRun = false,
        .dmaClrAct = false,
        .enable = false,
        .fallAction = timerInputActionStop,
        .mode = timerModeUp,
        .oneShot = true,
        .prescale = timerPrescale1024,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_Init(TIMER1, &timerInit);

    // Enable the overflow interrupt
    TIMER_IntEnable(TIMER1, TIMER_IEN_OF);

    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);

    // Power it down until we need it
    CMU_ClockEnable(cmuClock_TIMER1, false);
}

/**
 * Delay execution for a period of time (will drop to sleep and interrupts
 * will not be suppressed)
 *
 * @param ms Time in ms to delay for. Do not exceed 3 seconds
 */
void misc_delay(uint16_t ms)
{
    // Calculate how many ticks this is
    uint16_t delay_ticks = get_ticks_from_ms(ms, 1024);

    // Power up the timer
    CMU_ClockEnable(cmuClock_TIMER1, true);

    // Keep clocks on
    power_set_minimum(PWR_DELAY, PWR_EM1);

    // Indicate we have a delay in progress
    _delay_active = true;

    // Set calculated wrap value and start
    TIMER_TopSet(TIMER1, delay_ticks);
    TIMER_Enable(TIMER1, true);

    // Wait for timeout
    while (_delay_active)
    {
        power_sleep();
    }

    // Make sure the timer stopped, power it down
    TIMER_Enable(TIMER1, false);
    CMU_ClockEnable(cmuClock_TIMER1, false);

    // Remove our requirement to keep the clocks up
    power_set_minimum(PWR_DELAY, PWR_EM3);
}

/**
 * Handle timer expiry - clear the delay flag
 */
void TIMER1_IRQHandler(void)
{
    _delay_active = false;

    TIMER_IntClear(TIMER1, TIMER_IFC_OF);
}
