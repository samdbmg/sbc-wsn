/**
 * Minimum power mode identification system and power and clock management
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "power_management.h"

// Storage for all the power management bits
static power_system_store_t systems_wake = 0x0;
static power_system_store_t systems_sleep = 0x0;

// Store count of subsystems using GPIOD to keep it up
volatile uint8_t power_gpiod_use_count = 0;

// Scheduled function to run next we sleep
static void (*sched_func)(void);


/**
 * Allow a system to indicate the minimum power state it can currently operate
 * at
 *
 * @param system  The subsystem this request originates from
 * @param minimum The minimum permissible state
 */
void power_set_minimum(power_system_t system, power_min_t minimum)
{
    // First turn off all the state bits for this system
    systems_sleep &= (power_system_store_t)(~(0x1u << system));

    // Set the state bits appropriately
    switch (minimum)
    {
        case (PWR_WAKE):
        {
            systems_sleep |= (power_system_store_t)(0x1 << system);
            break;
        }
        case (PWR_SLEEP):
        {
            systems_sleep |= (power_system_store_t)(0x1 << system);
            break;
        }
        default:
            // Nothing to do in stop mode case as bits already cleared
            break;
    }
}

/**
 * Put the system into the maximum power mode requested by any system
 */
void power_sleep(void)
{
    // If there's a scheduled function, run it and return (sleep will be
    // called in a loop)
    if (sched_func)
    {
        // This enables the scheduled function to set another one if it has to
        void (*fn)(void) = sched_func;
        sched_func = 0x0;
        fn();
    }
    else
    {
        // Nothing scheduled, sleep
        if (systems_wake)
        {
            // Nothing to do, as we've requested full power, although this is odd...
        }
        else if (systems_sleep)
        {
            // Drop to sleep and wait for interrupt
            __WFI();
        }
        else
        {
            // No subsystems have requested a power mode, go to stop mode
            PWR_EnterSTOPMode(PWR_LowPowerRegulator_ON, PWR_STOPEntry_WFI);

            // Restart the HSE and wait for startup
            RCC_HSEConfig(RCC_HSE_ON);
            RCC_WaitForHSEStartUp();

            // Restart the PLL and wait for startup
            RCC_PLLCmd(ENABLE);
            while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
            {

            }

            RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

            // Stop the HSI
            RCC_HSICmd(DISABLE);
        }
    }
}

/**
 * Schedule a function to run next we try and sleep (like a low-priority
 * interrupt)
 *
 * @param fn Function pointer to execute.
 */
void power_schedule(void (*fn)(void))
{
    sched_func = fn;
}
