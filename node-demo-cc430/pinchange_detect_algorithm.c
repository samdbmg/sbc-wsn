/**
 * Detection algorithm for use with pin change method
 */

#include "driverlib.h"

#include "pin_mode.h"

// Detection algorithm parameters
#define EDGE_THRESHOLD_LOWMODE    20
#define EDGE_LOWER_BOUND_HIGHMODE 30
#define EDGE_UPPER_BOUND_HIGHMODE 45
#define CLICKS_LOWER_BOUND        4
#define CLICKS_UPPER_BOUND        9

// Detection status possible values
#define DETECT_IDLE       0
#define DETECT_FIRST_HIGH 1
#define DETECT_HIGH       2
#define DETECT_LOW        3
#define DETECT_WAIT       4

// How many clicks has the algorithm seen in this call?
static volatile uint8_t detected_clicks;
// Are we detecting clicks (true) or gaps (false)?
static volatile uint8_t detect_state = DETECT_IDLE;

uint8_t ticks_array[16];
uint8_t tick_recorder = 0;
uint32_t valid_hits = 0;

/* File-local functions */
static void disable_timers(void);
static void record_call(void);

/**
 * Check if detection algorithm is currently running
 * @return True if we should stay in EM1 with high-speed clocks running
 */
bool get_detect_active(void)
{
    return !(detect_state == DETECT_IDLE);
}

/**
 * We got a rising edge from the sensor
 */
void initial_detect(void)
{
    // Reconfigure to count edges
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN0);

    // Check whether this is a new call or part of an ongoing
    if (detect_state == DETECT_IDLE)
    {
        // OK, this is a new call hit. Start the timers, clear everything
        detected_clicks = 0;

        // Reset timer top and compare values
        Timer_A_stop(TIMER_A0_BASE);
        Timer_A_setCompareValue(TIMER_A0_BASE,
                TIMER_A_CAPTURECOMPARE_REGISTER_0, get_ticks_from_ms(1));
        Timer_A_setCompareValue(TIMER_A0_BASE,
                TIMER_A_CAPTURECOMPARE_REGISTER_1, get_ticks_from_ms(0.2));
        TA0R = 0;

        // Start the timers
        Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
        Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_CONTINUOUS_MODE);

        // Mark that we're at the start of detection
        detect_state = DETECT_FIRST_HIGH;
    }
    else if (detect_state == DETECT_WAIT)
    {
        // This is part of an ongoing call, has it started at least 1.5ms after
        // last ended.
        uint16_t value = TA0R;
        if (value > get_ticks_from_ms(1.5))
        {
            // This can be part of the last call, so run detect again
            detect_state = DETECT_HIGH;

            // Set the timer TOP and CC0 to 1ms and 200us in future
            Timer_A_stop(TIMER_A0_BASE);
            uint32_t countvalue = TA0R;
            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0,
                    countvalue + get_ticks_from_ms(1));
            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1,
                    countvalue + get_ticks_from_ms(0.2));

            // Re-enable CC0
            Timer_A_enableCaptureCompareInterrupt(TIMER_A0_BASE,
                    TIMER_A_CAPTURECOMPARE_REGISTER_1);

            Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
        }
        else
        {
            // If not its too early, ignore as a spurious wake (noise?)
            // Turn interrupt back on and wait for another
            GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
        }
    }
}

/**
 * 200us elapsed after first detection hit, check we're still getting edges
 */
void short_timeout(void)
{
    Timer_A_stop(TIMER_A1_BASE);
    uint32_t value = TA1R;
    Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_CONTINUOUS_MODE);

    if (value < 3)
    {
        // We're not getting continuous 40kHz pulses, so this is probably a
        // spurious wakeup from noise
        if (detect_state == DETECT_FIRST_HIGH)
        {
            // At this point we can just reset everything and go idle
            detected_clicks = 0;
            detect_state = DETECT_IDLE;

            tick_recorder = 0;

            // Power down the timers and re-arm the edge interrupt
            disable_timers();
        }
        else if (detect_state == DETECT_HIGH)
        {
            // Go and wait for a rising edge, but keep the timer going for timeout
            detect_state = DETECT_WAIT;

            // Clear the edge counter since this one was noise
            TA1R = 0;

            // Extend the time we will wait for the next click to be 2.5ms total
            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0,
                    get_ticks_from_ms(1.5));

            // Re-arm rising edge interrupt
            GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
        }
    }
    else
    {
        // Prevent this from running again
        TA0CCTL1 &= ~CCIE;
    }
}

/**
 * Main timer has run out, advance the detection state machine
 */
void full_timeout(void)
{
    Timer_A_stop(TIMER_A1_BASE);
    uint32_t value = TA1R;

    if (detect_state == DETECT_HIGH || detect_state == DETECT_FIRST_HIGH)
    {
        ticks_array[tick_recorder] = value;
        tick_recorder++;

        // Check we got enough edges for a 40kHz click
        if ((value >= EDGE_LOWER_BOUND_HIGHMODE) &&
                (value <= EDGE_UPPER_BOUND_HIGHMODE) &&
                (detected_clicks <= CLICKS_UPPER_BOUND))
        {
            // Next we need to detect a period of silence
            detect_state = DETECT_LOW;

            // Reset the counter
            TA1R = 0;

            // Set the timer back to 1ms
            Timer_A_stop(TIMER_A0_BASE);
            TA0R = 0;
            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0,
                    get_ticks_from_ms(1));
            Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

            // Start the counter
            Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_CONTINUOUS_MODE);
        }
        else
        {
            // We didn't get enough edges to count as a call, so reset
            detect_state = DETECT_IDLE;

            detected_clicks = 0;

            // If we got a valid call anyway, record it
            if (detected_clicks >= CLICKS_LOWER_BOUND &&
                    detected_clicks <= CLICKS_UPPER_BOUND)
            {
                record_call();
            }

            // Power down the timers and re-arm the edge interrupt
            disable_timers();
        }
    }
    else if (detect_state == DETECT_LOW)
    {
        ticks_array[tick_recorder] = value;
        tick_recorder++;

        // Check if its been quiet...
        if (value <= EDGE_THRESHOLD_LOWMODE)
        {
            // Switch to wait state and wait for next click in call
            detect_state = DETECT_WAIT;

            // Record the click we just got
            detected_clicks++;

            // Extend the timer top value to 2.5ms timeout
            Timer_A_stop(TIMER_A0_BASE);
            TA0R = get_ticks_from_ms(1);
            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0,
                    get_ticks_from_ms(2.5));
            Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

            // Re-arm the rising edge interrupt
            GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);

            // Reset the click counter and re-enable
            TA1R = 0;
            Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_CONTINUOUS_MODE);
        }
        else
        {
            // Too loud to count as a call, so reset
            detect_state = DETECT_IDLE;

            detected_clicks = 0;

            // If we got a valid call anyway, record it
            if (detected_clicks >= CLICKS_LOWER_BOUND &&
                    detected_clicks <= CLICKS_UPPER_BOUND)
            {
                record_call();
            }

            // Power down the timers and re-arm the edge interrupt
            disable_timers();
        }
    }
    else if (detect_state == DETECT_WAIT)
    {
        // We waited for another call but didn't get one, go back to idle

        // If we got a valid call anyway, record it
        if (detected_clicks >= CLICKS_LOWER_BOUND &&
                detected_clicks <= CLICKS_UPPER_BOUND)
        {
            record_call();
        }

        detected_clicks = 0;
        detect_state = DETECT_IDLE;

        disable_timers();

    }
}

/**
 * Power down all the timers
 */
static void disable_timers(void)
{
    tick_recorder = 0;

    // Power off the main 1ms timer
    Timer_A_stop(TIMER_A0_BASE);
    Timer_A_clearTimerInterruptFlag(TIMER_A0_BASE);
    Timer_A_clearCaptureCompareInterruptFlag(TIMER_A0_BASE,
            TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_clearCaptureCompareInterruptFlag(TIMER_A0_BASE,
            TIMER_A_CAPTURECOMPARE_REGISTER_1);
    TA0R = 0;

    // Clear the counter
    Timer_A_stop(TIMER_A1_BASE);
    TA1R = 0;

    // Re-enable the pin change interrupt
    GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
}

/**
 * Record that we just got a call
 */
static void record_call(void)
{
    //GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
    valid_hits++;

}
