/**
 * CC430 demo software for pin read/detect algorithm
 */

/* TI peripheral driver library */
#include "driverlib.h"

/* Application-specific includes */
#include "pin_mode.h"

/* File-local functions */
void set_clock_12mhz(void);

/**
 * Reconfigure the clock to run at 12MHz
 */
void set_clock_12mhz(void)
{
    // Set DCO FLL reference = REFO
    UCSCTL3 |= SELREF_2;

    // Disable the FLL control loop
    __bis_SR_register(SCG0);
    // Set lowest possible DCOx, MODx
    UCSCTL0 = 0x0000;
    // Select DCO range 24MHz operation
    UCSCTL1 = DCORSEL_5;

    /* Set DCO multiplier to get 12MHz
     * Fdco = (N + 1) * FLLRef, therefore
     * 12Mhz = (374 + 1) * 32768 therefore N=374
     * Also set FLL Div 2
     */
    UCSCTL2 = FLLD_1 + 374;

    // Enable the FLL control loop
    __bic_SR_register(SCG0);

    // Allow settlignt time as described in user manual
    __delay_cycles(375000);

    // Clear oscillatr fault flags
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
    SFRIFG1 &= ~OFIFG;
}

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

    // Run the clock a bit faster
    set_clock_12mhz();

    // Enable a pin for output
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    // Configure the detect algorithm peripherals
    pin_mode_init();

    //Enter LPM0, enable interrupts
    __bis_SR_register(GIE);

    while (1)
    {
        sleep_handler();
    }
}
