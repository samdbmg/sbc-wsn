/**
 * Hardware configuration for rising edge detect algorithm
 */

/* Peripheral driver library */
#include "driverlib.h"

/* Application specific includes */
#include "pin_mode.h"
#include "pinchange_detect_algorithm.h"

/* Functions only in this file */
static void edge_counter_init(void);
static void edge_detect_init(void);
static void click_timer_init(void);


/**
 * Call the rest of the initialisation functions in this file
 */
void pin_mode_init(void)
{
    edge_detect_init();
    edge_counter_init();
    click_timer_init();
}

/**
 * Configure a timer to count on rising edges
 */
void edge_counter_init(void)
{
    Timer_A_initContinuousModeParam timer_init_data = {0};

    timer_init_data.clockSource = TIMER_A_CLOCKSOURCE_EXTERNAL_TXCLK;
    timer_init_data.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    timer_init_data.startTimer = false;
    timer_init_data.timerClear = TIMER_A_DO_CLEAR;
    timer_init_data.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;

    Timer_A_initContinuousMode(TIMER_A1_BASE, &timer_init_data);
}

/**
 * Configure a GPIO pin (P2.0) to wake the CPU on rising edges
 */
void edge_detect_init(void)
{
    GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
    GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
    GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
}

/**
 * Set up the 1ms click timer
 */
void click_timer_init(void)
{
    Timer_A_initUpModeParam timer_init_data = {0};

    timer_init_data.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE;
    timer_init_data.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    timer_init_data.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_4;
    timer_init_data.startTimer = false;
    timer_init_data.timerClear = TIMER_A_DO_CLEAR;
    timer_init_data.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    timer_init_data.timerPeriod = get_ticks_from_ms(1);

    Timer_A_initUpMode(TIMER_A0_BASE, &timer_init_data);

    Timer_A_initCompareModeParam timer_compare_data = {0};

    timer_compare_data.compareInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE;
    timer_compare_data.compareOutputMode = TIMER_A_OUTPUTMODE_OUTBITVALUE;
    timer_compare_data.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1;
    timer_compare_data.compareValue = get_ticks_from_ms(0.2);

    Timer_A_initCompareMode(TIMER_A0_BASE, &timer_compare_data);

}

void sleep_handler(void)
{
    if (get_detect_active() == true)
    {
        __low_power_mode_0();
    }
    else
    {
        __low_power_mode_4();
    }
}


/**
 * Handle rising edge detection, trigger algorithm
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(PORT2_VECTOR)))
#endif
void Port_2(void)
{
    // Ensure we transition power mode correctly
    __low_power_mode_off_on_exit();

    if (GPIO_getInterruptStatus(GPIO_PORT_P2, GPIO_PIN0))
    {
        initial_detect();

        GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
    }
}

/**
 * Handle the timer hitting a capture compare interrupt for full 1ms timeout
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(TIMER0_A0_VECTOR)))
#endif
void TIMER0_A0_ISR(void)
{
    // Ensure we transition power mode correctly
    __low_power_mode_off_on_exit();

    full_timeout();
    Timer_A_clearCaptureCompareInterruptFlag(TIMER_A0_BASE,
            TIMER_A_CAPTURECOMPARE_REGISTER_0);
    Timer_A_clearTimerInterruptFlag(TIMER_A0_BASE);
}

/**
 * Handle the timer hitting a capture compare interrupt for a short timeout
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A1_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(TIMER0_A1_VECTOR)))
#endif
void TIMER0_A1_ISR(void)
{
    // Do nothing unless we got a CC1 interrupt
    uint16_t test = TA0IV;
    if (test == 0x02)
    {
        // Ensure we transition power mode correctly
        __low_power_mode_off_on_exit();

        short_timeout();
        Timer_A_clearCaptureCompareInterruptFlag(TIMER_A0_BASE,
                TIMER_A_CAPTURECOMPARE_REGISTER_1);
    }

    Timer_A_clearTimerInterruptFlag(TIMER_A0_BASE);
}
