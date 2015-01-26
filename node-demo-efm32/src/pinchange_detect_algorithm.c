/**
 * Detection algorithm for use with pin change method
 */

#include "em_timer.h"
#include "em_cmu.h"
#include "em_gpio.h"

// Detection algorithm parameters
#define EDGE_THRESHOLD_LOWMODE    10
#define EDGE_LOWER_BOUND_HIGHMODE 30
#define EDGE_UPPER_BOUND_HIGHMODE 45
#define CLICKS_LOWER_BOUND        4
#define CLICKS_UPPER_BOUND        9

// Detection status possible values
#define DETECT_WAITING 0
#define DETECT_HIGH    1
#define DETECT_LOW     2

// How many clicks has the algorithm seen in this call?
static volatile uint8_t detected_clicks;
// Are we detecting clicks (true) or gaps (false)?
static volatile uint8_t detect_state;

uint8_t ticks_array[16];
uint8_t tick_recorder = 0;
uint32_t valid_hits = 0;

static uint32_t timer_ticks_1ms;
static uint32_t timer_ticks_2ms;

/* File-local functions */
static void disable_timers(void);
static void record_call(void);

/**
 * Configure some timeouts for detection setup
 * @param ticks_1ms Number of timer ticks for the high side
 * @param ticks_2ms Number of timer ticks for the low side
 */
void setup_detect_algorithm(uint32_t ticks_1ms, uint32_t ticks_2ms)
{
    timer_ticks_1ms = ticks_1ms;
    timer_ticks_2ms = ticks_2ms;
}

/**
 * Check if detection algorithm is currently running
 * @return True if we should stay in EM1 with high-speed clocks running
 */
bool get_detect_active(void)
{
    return detect_state != DETECT_WAITING;
}

/**
 * We got a rising edge from the sensor
 */
void initial_detect(void)
{
    // Disable the interrupt that triggers this until the algorithm finishes
    GPIO_IntDisable(1);

    // Power up and enable the timers that make this work
    CMU_ClockEnable(cmuClock_TIMER1, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_TIMER0, true);

    // Mark that detection is running
    detect_state = DETECT_HIGH;

    // Turn the timers back on to actually run detection
    TIMER_Enable(TIMER0, true);
    TIMER_Enable(TIMER1, true);
    TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
    NVIC_EnableIRQ(TIMER0_IRQn);
}

/**
 * 200us elapsed after first detection hit, check we're still getting edges
 */
void short_timeout(void)
{
    uint32_t value = TIMER_CounterGet(TIMER1);

    if (value < 3)
    {
        // We're not getting continuous 40kHz pulses, so this is probably a
        // spurious wakeup from noise
        disable_timers();
    }
    else
    {
        // Prevent this from running again
        TIMER_IntDisable(TIMER0, TIMER_IEN_CC0);
    }
}

/**
 * Main timer has run out, advance the detection state machine
 */
void full_timeout(void)
{
    uint32_t value = TIMER_CounterGet(TIMER1);

    ticks_array[tick_recorder] = value;
    tick_recorder++;

    // Are we expecting a click or silence?
    if (detect_state == DETECT_HIGH)
    {
        // Expected a click, check we got enough edges
        if ((value >= EDGE_LOWER_BOUND_HIGHMODE) &&
                (value <= EDGE_UPPER_BOUND_HIGHMODE) &&
                (detected_clicks <= CLICKS_UPPER_BOUND))
        {
            detect_state = DETECT_LOW;

            // Reconfigure the timer to count for 2ms
            TIMER_TopSet(TIMER0, timer_ticks_2ms);
            TIMER_CounterSet(TIMER0, 0);

            // Reset the counter
            TIMER_CounterSet(TIMER1, 0);
        }
        else
        {
            // Check if we got enough calls to record this one
            if (detected_clicks >= CLICKS_LOWER_BOUND)
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
        if (value <= EDGE_THRESHOLD_LOWMODE)
        {
            // Record that we got a valid call
            detected_clicks++;

            // Set up for high side measurement
            detect_state = DETECT_HIGH;

            // Reconfigure the timer to count for 1ms
            TIMER_TopSet(TIMER0, timer_ticks_1ms);
            TIMER_CounterSet(TIMER0, 0);

            // Reset the counter
            TIMER_CounterSet(TIMER1, 0);

        }
        else
        {
            if (detected_clicks >= CLICKS_LOWER_BOUND)
            {
                record_call();
            }

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
    detect_state = DETECT_WAITING;

    tick_recorder = 0;

    // Reconfigure the timer back to 1ms and turn it off
    TIMER_TopSet(TIMER0, timer_ticks_1ms);
    TIMER_CompareSet(TIMER0, 0, timer_ticks_1ms/5);
    TIMER_Enable(TIMER0, false);
    TIMER_IntClear(TIMER0, TIMER_IF_CC0 | TIMER_IF_OF);
    CMU_ClockEnable(cmuClock_TIMER0, false);
    NVIC_DisableIRQ(TIMER0_IRQn);
    //NVIC_ClearPendingIRQ(TIMER0_IRQn);

    // Clear the counter
    TIMER_Enable(TIMER1, false);
    TIMER_CounterSet(TIMER1, 0);

    // Turn off the counter
    CMU_ClockEnable(cmuClock_TIMER1, false);
    CMU_ClockEnable(cmuClock_PRS, false);

    // Re-enable the pin change interrupt
    GPIO_IntClear(1);
    GPIO_IntEnable(1);
}

/**
 * Record that we just got a call
 */
static void record_call(void)
{
    valid_hits++;
}
