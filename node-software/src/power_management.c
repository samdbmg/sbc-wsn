/**
 * Minimum power mode identification system
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_emu.h"

/* Application-specific headers */
#include "power_management.h"

// Storage for all the power management bits
static power_system_store_t systems_em0 = 0x0;
static power_system_store_t systems_em1 = 0x0;
static power_system_store_t systems_em2 = 0x0;

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
    systems_em0 &= ~(0x1 << system);
    systems_em1 &= ~(0x1 << system);
    systems_em2 &= ~(0x1 << system);

    // Set the state bits appropriately
    switch (minimum)
    {
        case (PWR_EM0):
        {
            systems_em0 |= (0x1 << system);
            break;
        }
        case (PWR_EM1):
        {
            systems_em1 |= (0x1 << system);
            break;
        }
        case (PWR_EM2):
        {
            systems_em2 |= (0x1 << system);
            break;
        }
        default:
            // Nothing to do in EM3 case as bits already cleared
            break;
    }
}

/**
 * Put the system into the maximum power mode requested by any system
 */
void power_sleep(void)
{
    if (systems_em0)
    {
        // Nothing to do, as we've requested full power, although this is odd...
    }
    else if (systems_em1)
    {
        EMU_EnterEM1();
    }
    else if (systems_em2)
    {
        EMU_EnterEM2(true);
    }
    else
    {
        // No subsystems have requested a power mode, sleep!
        EMU_EnterEM3(true);
    }
}
