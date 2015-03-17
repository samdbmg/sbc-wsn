/**
 * Minimum power mode identification system
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
    }
}
