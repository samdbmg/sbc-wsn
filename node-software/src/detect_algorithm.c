/**
 * Speckled Bush-cricket Call Detection Algorithm
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_timer.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_acmp.h"
#include "em_gpio.h"
#include "em_rtc.h"

/* Application-specific headers */
#include "detect_algorithm.h"
#include "misc.h"
#include "power_management.h"
#include "detect_data_store.h"
#include "status_leds.h"
#include "printf.h"

/* Functions used only in this file */
static void _detect_timer_config(void);
static void _detect_comparator_config(void);
static void _detect_reset_to_idle(void);
static void _detect_transient_handler(void);
static void _detect_start_new(void);

/* Variable declarations */
static uint8_t call_count;
static uint8_t detect_state;
static uint8_t transient_count;

#ifdef DETECT_DEBUG_ON
uint16_t debug_data_array[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#endif

/**
 * Call the internal functions to start up the SBC detection algorithm
 */
void detect_init(void)
{
    _detect_timer_config();
    _detect_comparator_config();

    call_count = 0;
    detect_state = DETECT_IDLE;
}


/**
 * Set up a timer for the detection system to use, default to the first state
 */
static void _detect_timer_config(void)
{
    // Run clock to the timer
    CMU_ClockEnable(cmuClock_TIMER0, true);

    const TIMER_Init_TypeDef timerInit =
    {
        .clkSel = timerClkSelHFPerClk,
        .debugRun = false,
        .dmaClrAct = false,
        .enable = false,
        .fallAction = timerInputActionStop,
        .mode = timerModeUp,
        .oneShot = false,
        .prescale = timerPrescale16,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_Init(TIMER0, &timerInit);

    // Wrap around is about 1kHz
    TIMER_TopSet(TIMER0, DETECT_HIGH_UB);

    // Enable the overflow interrupt
    TIMER_IntEnable(TIMER0, TIMER_IEN_OF);

    NVIC_ClearPendingIRQ(TIMER0_IRQn);
    NVIC_EnableIRQ(TIMER0_IRQn);

    // Power it down until we need it
    CMU_ClockEnable(cmuClock_TIMER0, false);
}

/**
 * Configure the Analog Comparator to detect edges on the input. Based on code
 * given in an0020_efm32_analog_comparator example.
 */
static void _detect_comparator_config(void)
{
    // Supply a clock to the comparator
    CMU_ClockEnable(cmuClock_ACMP0, true);

    const ACMP_Init_TypeDef acmp_settings =
    {
        false,                              // Full bias current
        false,                              // Half bias current
        9,                                   // Biasprog current configuration
        false,                              // Enable interrupt for falling edge
        true,                               // Enable interrupt for rising edge
        acmpWarmTime256,                    // Warm-up time in clock cycles, >140 cycles for 10us with 14MHz
        acmpHysteresisLevel7,               // Hysteresis configuration
        0,                                  // Inactive comparator output value
        true,                               // Enable low power mode
        0,                                  // Vdd reference scaling
        true,                               // Enable ACMP
    };

    ACMP_Init(ACMP0, &acmp_settings);

    // Set the negative input as channel 2, positive as channel 3
    ACMP_ChannelSet(ACMP0, acmpChannel3, acmpChannel2);

    // Wait for comparator to finish starting up
    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT))
    {

    }

    // Clear interrupt flags
    ACMP_IntClear(ACMP0, ACMP_IFC_WARMUP);
    ACMP_IntClear(ACMP0, ACMP_IFC_EDGE);

    // Activate edge trigger interrupt
    ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);

    // Enable interrupts
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_EnableIRQ(ACMP0_IRQn);

}

/**
 * Handle timeout by resetting the timer back to defaults and returning to IDLE
 */
void TIMER0_IRQHandler(void)
{
    TIMER_IntClear(TIMER0, TIMER_IFC_OF);

#ifdef DETECT_DEBUG_ON
    if (detect_state == DETECT_HIGH)
    {
        debug_data_array[2*call_count] = TIMER_TopGet(TIMER0);
    }
    else
    {
        debug_data_array[2*call_count + 1] = TIMER_TopGet(TIMER0);
    }
#endif

    // If we're in the last stage of female detection, timeout means success
    if (detect_state == DETECT_LOW_F)
    {
    	status_led_set(STATUS_YELLOW, true);
    	store_call(true);
    	printf("Detect hit (and female) with %d clicks", call_count);
    	call_count = 0;
    	_detect_reset_to_idle();
    }
    // If we got enough hits, enable female detect mode
    else if (call_count >= DETECT_MINCOUNT)
    {
    	detect_state = DETECT_WAIT_F;

        // Set edge trigger to fire on a rising edge
        ACMP0->CTRL &= ~ACMP_CTRL_IFALL;
        ACMP0->CTRL |= ACMP_CTRL_IRISE;

        // Reset the timers
        TIMER_Enable(TIMER0, false);
        TIMER_CounterSet(TIMER0, 0);
        TIMER_TopSet(TIMER0, DETECT_WAIT_F_UB);
        TIMER_CounterSet(TIMER0, DETECT_LOW_UB);
        TIMER_Enable(TIMER0, true);

    }
    else
    {
		// Stop and reset the timer for next detect
		_detect_reset_to_idle();
    }
}

/**
 * Handle a comparator edge, run state machine
 */
void ACMP0_IRQHandler(void)
{
    uint32_t timer_val = TIMER_CounterGet(TIMER0);

    switch(detect_state)
    {
        case DETECT_IDLE:
            _detect_start_new();
            break;

        case DETECT_HIGH:
            // Did this falling edge arrive approx 1ms after the rise?
            if (timer_val > DETECT_HIGH_LB)
            {
#ifdef DETECT_DEBUG_ON
                debug_data_array[2 * call_count] = timer_val;
#endif

                detect_state = DETECT_LOW;

                // Set edge trigger to fire on a rising edge
                ACMP0->CTRL &= ~ACMP_CTRL_IFALL;
                ACMP0->CTRL |= ACMP_CTRL_IRISE;

                // Reset the timers again
                TIMER_Enable(TIMER0, false);
                TIMER_CounterSet(TIMER0, 0);
                TIMER_TopSet(TIMER0, DETECT_LOW_UB);
                TIMER_Enable(TIMER0, true);
            }
            else
            {
                _detect_transient_handler();
            }
            break;

        case DETECT_LOW:
            // Did this rising edge arrive approx 2ms after the fall?
            if (timer_val > DETECT_LOW_LB)
            {
#ifdef DETECT_DEBUG_ON
                debug_data_array[2 * call_count + 1] = timer_val;
#endif

                // We got a complete pulse, mark a click
                call_count++;

                // If we now have a full call, enter female mode
                if (call_count >= DETECT_MAXCOUNT)
                {
					detect_state = DETECT_WAIT_F;

					// Set edge trigger to fire on a rising edge
					ACMP0->CTRL &= ~ACMP_CTRL_IFALL;
					ACMP0->CTRL |= ACMP_CTRL_IRISE;

					// Reset the timers
					TIMER_Enable(TIMER0, false);
					TIMER_CounterSet(TIMER0, 0);
					TIMER_TopSet(TIMER0, DETECT_WAIT_F_UB);
					TIMER_Enable(TIMER0, true);
                }
                else
                {
                    detect_state = DETECT_HIGH;

                    // Set edge trigger to fire on a falling edge
                    ACMP0->CTRL &= ~ACMP_CTRL_IRISE;
                    ACMP0->CTRL |= ACMP_CTRL_IFALL;

                    // Reset the timers again
                    TIMER_Enable(TIMER0, false);
                    TIMER_CounterSet(TIMER0, 0);
                    TIMER_TopSet(TIMER0, DETECT_HIGH_UB);
                    TIMER_Enable(TIMER0, true);
                }
            }
            else
            {
                _detect_transient_handler();
            }
            break;
        case DETECT_WAIT_F:
        	if (timer_val > DETECT_WAIT_F_LB)
        	{
        		// Ok, this was either a female or a new call coming in, let's wait for the low
				detect_state = DETECT_HIGH_F;

                // Set edge trigger to fire on a falling edge
                ACMP0->CTRL &= ~ACMP_CTRL_IRISE;
                ACMP0->CTRL |= ACMP_CTRL_IFALL;

                // Reset the timers again
                TIMER_Enable(TIMER0, false);
                TIMER_CounterSet(TIMER0, 0);
                TIMER_TopSet(TIMER0, DETECT_HIGH_UB);
                TIMER_Enable(TIMER0, true);
        	}
        	else if (timer_val > 1181)
        	{
        		// We came in too early so it's probably a new detect. Save the old, start again
        		store_call(false);
        		printf("Detect hit with %d clicks", call_count);
        		status_led_set(STATUS_YELLOW, true);
        		_detect_start_new();
        	}
        	else
        	{
        		_detect_transient_handler();
        	}
        	break;
        case DETECT_HIGH_F:
        	if (timer_val > DETECT_HIGH_LB)
        	{
        		// Now we wait to see if this was a new call or just a female
				detect_state = DETECT_LOW_F;

                // Set edge trigger to fire on a rising edge
                ACMP0->CTRL &= ~ACMP_CTRL_IRISE;
                ACMP0->CTRL |= ACMP_CTRL_IFALL;

                // Reset the timers again
                TIMER_Enable(TIMER0, false);
                TIMER_CounterSet(TIMER0, 0);
                TIMER_TopSet(TIMER0, DETECT_LOW_UB);
                TIMER_Enable(TIMER0, true);
        	}
        	else
        	{
        		// Transient probably
        		_detect_transient_handler();
        	}
        	break;
        case DETECT_LOW_F:
        	// If we got a click here it wasn't a female, another call started.
        	// Save the old one
        	store_call(false);
        	printf("Detect hit with %d clicks", call_count);
        	status_led_set(STATUS_YELLOW, true);

        	// Reset as if we'd seen the new call
        	_detect_start_new();

        	call_count++;

        	// And shortcut directly as if we just left DETECT_HIGH
            detect_state = DETECT_LOW;

            // Set edge trigger to fire on a rising edge
            ACMP0->CTRL &= ~ACMP_CTRL_IFALL;
            ACMP0->CTRL |= ACMP_CTRL_IRISE;

            // Reset the timers again
            TIMER_Enable(TIMER0, false);
            TIMER_CounterSet(TIMER0, 0);
            TIMER_TopSet(TIMER0, DETECT_LOW_UB);
            TIMER_Enable(TIMER0, true);
            break;
    }
    // Clear interrupt flag
    ACMP_IntClear(ACMP0, ACMP_IFC_EDGE);
}

/**
 * Begin a new detection operation
 */
static void _detect_start_new(void)
{
	detect_state = DETECT_HIGH;

#ifdef DETECT_DEBUG_ON
	printf("Detect debug: ");
    for (uint8_t i = 0; i < 16; i++)
    {
    	printf("%d, ", debug_data_array[i]);
        debug_data_array[i] = 0;
    }
    printf("\r\n");
#endif

    // Reset counter
    call_count = 0;
    transient_count = 0;

	// Set edge trigger to fire on a falling edge
	ACMP0->CTRL &= ~ACMP_CTRL_IRISE;
	ACMP0->CTRL |= ACMP_CTRL_IFALL;

	// Power the timer back up
	CMU_ClockEnable(cmuClock_TIMER0, true);

	// Indicate we need to stay awake to keep the timer on
	power_set_minimum(PWR_DETECT, PWR_EM1);

	// Reset the timers
	TIMER_Enable(TIMER0, false);
	TIMER_CounterSet(TIMER0, 0);
	TIMER_TopSet(TIMER0, DETECT_HIGH_UB);
	TIMER_Enable(TIMER0, true);
}

/**
 * A transient edge was detected that was too soon for a call
 */
static void _detect_transient_handler(void)
{
    // This was a transient - a few are fine, lots mean we're
    // probably hearing something else
    transient_count++;

    if (transient_count > DETECT_TRANSIENTTH)
    {
        // Reject and reset
        _detect_reset_to_idle();

    }
}

/**
 * Clear the timers and reset detection back to the default state
 */
static void _detect_reset_to_idle(void)
{
    // Stop and reset the timer for next detect
    TIMER_Enable(TIMER0, false);
    TIMER_CounterSet(TIMER0, 0);
    TIMER_TopSet(TIMER0, DETECT_HIGH_UB);

    // Kill timer power
    CMU_ClockEnable(cmuClock_TIMER0, false);

    // Mark a detection if we got enough
    if (call_count >= DETECT_MINCOUNT)
    {
    	status_led_set(STATUS_YELLOW, true);
        store_call(false);

        printf("Detect hit with %d clicks", call_count);
    }

    // Set edge trigger to fire on a rising edge
    ACMP0->CTRL &= ~ACMP_CTRL_IFALL;
    ACMP0->CTRL |= ACMP_CTRL_IRISE;

    // Reset state
    call_count = 0;
    detect_state = DETECT_IDLE;

    // Mark we're ready to go to sleep
    power_set_minimum(PWR_DETECT, PWR_EM3);
    status_led_set(STATUS_YELLOW, false);
}
