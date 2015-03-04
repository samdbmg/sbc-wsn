/**
 * CC430 demo software for pin read/detect algorithm
 */

/* TI peripheral driver library */
#include "driverlib.h"

/* Application-specific includes */
#include "pin_mode.h"

/* File-local functions */
static void set_clock_12mhz(void);
static void set_ports_lowpower(void);

/**
 * Reconfigure the clock to run at 12MHz
 */
static void set_clock_12mhz(void)
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

static void set_ports_lowpower(void)
{
    PADIR = 0xFFFF;
    PBDIR = 0xFFFF;
    PCDIR = 0xFFFF;
    PJDIR = 0xFFFF;
    PAOUT = 0x0000;
    PBOUT = 0x0000;
    PCOUT = 0x0000;
    PJOUT = 0x0000;

    P1OUT |= 0x82;
    P2OUT |= 0x03;
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
    PMM_setVCore(1);
    set_clock_12mhz();

    // Configure all ports
    set_ports_lowpower();

    // Enable a pin for output
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    // Configure the detect algorithm peripherals
    pin_mode_init();

    // Enable interrupts
    __bis_SR_register(GIE);

    while (1)
    {
        sleep_handler();
    }
}
