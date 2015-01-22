/**
 * Detection algorithm for use with pin change method
 */

#include "em_timer.h"
#include "em_cmu.h"

// How many clicks has the algorithm seen in this call?
static volatile uint8_t detected_clicks;
// Are we detecting clicks (true) or gaps (false)?
static volatile bool high_side;

static uint32_t timer_ticks_1ms;
static uint32_t timer_ticks_2ms;

/* File-local functions */
static void disable_timers(void);
static void record_call(void);

void setup_detect_algorithm(uint32_t ticks_1ms, uint32_t ticks_2ms)
{
    timer_ticks_1ms = ticks_1ms;
    timer_ticks_2ms = ticks_2ms;
}

/**
 * We got a rising edge from the sensor
 */
void initial_detect(void)
{
    // Disable the interrupt that triggers this until the algorithm finishes
    NVIC_DisableIRQ(GPIO_EVEN_IRQn);

    // Power up and enable the timers that make this work
    CMU_ClockEnable(cmuClock_TIMER1, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_TIMER0, true);

    TIMER_Enable(TIMER0, true);
    TIMER_IntEnable(TIMER1, TIMER_IF_CC0);
    TIMER_IntEnable(TIMER0, TIMER_IF_CC0);
    TIMER_CounterSet(TIMER1, 0);
}

/**
 * 100us elapsed after first detection hit, check we're still getting edges
 */
void short_timeout(void)
{
    if (TIMER_CounterGet(TIMER1) < 3)
    {
        // We're not getting continuous 40kHz pulses, so this is probably a
        // spurious wakeup from noise
        disable_timers();
    }
}

/**
 * Main timer has run out, advance the detection state machine
 */
void full_timeout(void)
{
    uint32_t value = TIMER_CounterGet(TIMER1);

    // Are we expecting a click or silence?
    if (high_side == true)
    {
        // Expected a click, check we got enough edges
        if ((value > 35 && value < 45) && detected_clicks < 9)
        {
            high_side = false;

            // Reconfigure the timer to count for 2ms
            TIMER_TopSet(TIMER0, timer_ticks_2ms);

            // Reset the counter
            TIMER_CounterSet(TIMER1, 0);
        }
        else
        {
            // Check if we got enough calls to record this one
            if (detected_clicks > 4)
            {
                record_call();
            }

            // Reset and shut down
            disable_timers();
        }
    }
    else
    {
        // Expected no click, check we got silence (ish)
        if (value < 5)
        {
            // Record that we got a valid call
            detected_clicks++;

            // Set up for high side measurement
            high_side = true;

            // Reconfigure the timer to count for 1ms
            TIMER_TopSet(TIMER0, timer_ticks_1ms);

            // Reset the counter
            TIMER_CounterSet(TIMER1, 0);

        }
        else
        {
            // Reset and shut down
            disable_timers();
        }
    }
}

/**
 * Power down all the timers and reset to wait
 */
static void disable_timers(void)
{
    // Reset ready for next run
    detected_clicks = 0;
    high_side = true;

    // Reconfigure the timer back to 1ms and turn it off
    TIMER_TopSet(TIMER0, timer_ticks_1ms);
    TIMER_Enable(TIMER0, false);
    CMU_ClockEnable(cmuClock_TIMER0, false);

    // Turn off the counter
    CMU_ClockEnable(cmuClock_TIMER1, false);
    CMU_ClockEnable(cmuClock_PRS, false);

    // Re-enable the pin change interrupt
    NVIC_EnableIRQ(GPIO_EVEN_IRQn);
}

/**
 * Record that we just got a call
 */
static void record_call(void)
{
    //TODO
}
