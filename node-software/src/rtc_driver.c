/**
 * Real time clock configuration and control
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_rtc.h"

/* Application-specific headers */
#include "rtc_driver.h"
#include "power_management.h"
#include "misc.h"

#define RTC_OSC_FREQ             (32768)
#define RTC_OSC_PSC_VAL          (32768)
#define RTC_TICK_RATE            (RTC_OSC_FREQ / RTC_OSC_PSC_VAL)
#define RTC_WAKE_INTERVAL        (2000)
#define RTC_TIMEOUT_INTERVAL     (6000)
#define RTC_COUNT_BEFORE_WAKEUP  (RTC_TICK_RATE* RTC_WAKE_INTERVAL)
#define RTC_COUNT_BEFORE_TIMEOUT (RTC_TICK_RATE * RTC_TIMEOUT_INTERVAL)

/**
 * Configure and start the real time counter
 */
void rtc_init(void)
{
    // Start the LFRCO (Low Freq RC Oscillator) and wait for it to stabilise
    // The ULFRCO uses less power, but is much less accurate!
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);

    // Set it as the RTC clock source and enable the RTC
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_RTC, true);

    // Set RTC prescaler
    CMU_ClockDivSet(cmuClock_RTC, RTC_OSC_PSC_VAL);

    // Set compare match value for RTC
    RTC_CompareSet(1, RTC_COUNT_BEFORE_WAKEUP);

    // Set top value
    RTC_CompareSet(0, RTC_COUNT_BEFORE_TIMEOUT);

    // Enable RTC interrupts
    RTC_IntEnable(RTC_IFC_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);

    // Configure, init and start the RTC
    RTC_Init_TypeDef rtcInit =
    {
            .enable = true,
            .comp0Top = true,
            .debugRun = true
    };

    RTC_Init(&rtcInit);

    // We can't drop to EM3 when using the ULFRCO or it will stop
    power_set_minimum(PWR_RTC, PWR_EM2);
}

/**
 * RTC timeout handler
 */
void RTC_IRQHandler(void)
{
    if (RTC_IntGet() & RTC_IF_COMP1)
    {
        // Hourly interrupt fired, calculate the next hour interrupt (and adjust
        // for crossing midnight with a mod
        uint32_t new_compare = RTC_CompareGet(1) + RTC_COUNT_BEFORE_WAKEUP;
        new_compare %= RTC_COUNT_BEFORE_TIMEOUT;
        RTC_CompareSet(1, new_compare);

        // Send a burst of data back on the radio


        // Clear flag for next time
        RTC_IntClear(RTC_IFC_COMP0);
    }
    else if (RTC_IntGet() & RTC_IF_COMP0)
    {
        // Daily interrupt fired, just clear the flag
        RTC_IntClear(RTC_IF_COMP1);
    }

}