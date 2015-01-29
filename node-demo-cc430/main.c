/**
 * CC430 demo software for pin read/detect algorithm
 */

/* TI peripheral driver library */
#include "driverlib.h"

/* Application-specific includes */
#include "pin_mode.h"

/**
 * Configure and start the RTC
 */
void rtc_init(void)
{

}



/**
 * Main function, actually do the work
 */
void main(void)
{
    //Stop watchdog timer
    WDT_A_hold(WDT_A_BASE);

    pin_mode_init();

    //Enter LPM0, enable interrupts
    __bis_SR_register(LPM0_bits + GIE);

    while (1)
    {

    }
}
