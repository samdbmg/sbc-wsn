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
#include "radio_protocol.h"

#define RTC_OSC_PSC_VAL          (32768)
#define RTC_TIMEOUT_INTERVAL     (86400)
#define RTC_COUNT_BEFORE_TIMEOUT (86400)

static uint32_t rtc_wake_period = RTC_COUNT_BEFORE_TIMEOUT;

/**
 * Configure and start the real time counter
 */
void rtc_init(void)
{
    // Enable RTC clock
    CMU_ClockEnable(cmuClock_RTC, true);

    // Set RTC prescaler
    CMU_ClockDivSet(cmuClock_RTC, RTC_OSC_PSC_VAL);

    // Set compare match value for RTC
    RTC_CompareSet(1, rtc_wake_period);

    // Set top value
    RTC_CompareSet(0, RTC_COUNT_BEFORE_TIMEOUT);

    // Enable RTC interrupts
    RTC_IntEnable(RTC_IFS_COMP0 | RTC_IFC_COMP1);
    NVIC_EnableIRQ(RTC_IRQn);

    // Configure, init and start the RTC
    RTC_Init_TypeDef rtcInit =
    {
            .enable = true,
            .comp0Top = true,
            .debugRun = false
    };

    RTC_Init(&rtcInit);

    // We can't drop to EM3 when using the ULFRCO or it will stop
    power_set_minimum(PWR_RTC, PWR_EM2);
}

bool rtc_get_time_16(uint16_t* time_p)
{
    uint32_t count = RTC_CounterGet();
    *time_p = count & 0xFFFF;
    return (0x10000 & count);
}

/**
 * Set the current real time clock value in seconds
 * @param timestamp Current time from upstream
 * @param msb	    Most significant bit (stored elsewhere)
 */
void rtc_set_time(uint16_t timestamp, uint8_t msb)
{
    RTC_Enable(false);
    RTC->CNT = (uint32_t) timestamp | ((uint32_t) msb << 16);
    RTC_Enable(true);

    // Calculate the next interrupt (and adjust for crossing midnight)
    uint32_t new_compare = RTC_CounterGet() + rtc_wake_period;
    new_compare %= RTC_COUNT_BEFORE_TIMEOUT;
    RTC_CompareSet(1, new_compare);
}

/**
 * Adjust the schedule for next wakeup. This should be called after
 * time has been set!
 *
 * @param period    Period between wakeups
 * @param next_wake Absolute time value at which to next wake
 */
void rtc_set_schedule(uint32_t period, uint32_t next_wake)
{
	rtc_wake_period = period;
	RTC_CompareSet(1, next_wake);
}

/**
 * RTC timeout handler
 */
void RTC_IRQHandler(void)
{
    if (RTC_IntGet() & RTC_IF_COMP1)
    {
        // Hourly interrupt fired, calculate the next hour interrupt (and adjust
        // for crossing midnight with a mod)
        uint32_t new_compare = RTC_CompareGet(1) + rtc_wake_period;
        new_compare %= RTC_COUNT_BEFORE_TIMEOUT;
        RTC_CompareSet(1, new_compare);

        // Send a burst of data back on the radio
        proto_triggerupload();

        // Clear flag for next time
        RTC_IntClear(RTC_IFC_COMP1);
    }
    else if (RTC_IntGet() & RTC_IF_COMP0)
    {
        // Daily interrupt fired, just clear the flag
        RTC_IntClear(RTC_IF_COMP0);
    }

}
