/**
 * EFM32 Node demo for pin change mode
 */

#include "em_device.h"
#include "em_prs.h"
#include "em_timer.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_rtc.h"
#include "em_usart.h"
#include "em_gpio.h"
#include "em_pcnt.h"

#include "pin_mode.h"
#include "pinchange_detect_algorithm.h"

bool timer_active = false;
bool last_step = false;

uint32_t timer_1ms_ticks;
uint32_t timer_2ms_ticks;

volatile uint64_t high_count;
volatile uint64_t low_count;
volatile uint32_t pulse_buffer;

/**
 * Call the init functions below
 */
void pin_mode_init(void)
{
    pin_timer_init();
    pinchange_init();
}

/**
 * Handle an RTC timeout by setting some values
 */
void rtc_handler(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        USART_SpiTransfer(USART1, high_count >> (i*8));
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        USART_SpiTransfer(USART1, low_count >> (i*8));
    }
}

/**
 * Set up a pin to increment a counter when the state changes
 */
void pinchange_init(void)
{
    CMU_ClockEnable(cmuClock_PRS, true);

    // Enable GPIO PC0 as input
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);

    // Configure GPIO PC0 to fire an interrupt on a rising edge
    GPIO_IntConfig(gpioPortC, 0, true, false, true);

    // Enable GPIO PRS sense
    GPIO_InputSenseSet(GPIO_INSENSE_PRS | GPIO_INSENSE_INT,
           GPIO_INSENSE_PRS | GPIO_INSENSE_INT);


    CMU_ClockEnable(cmuClock_TIMER1, true);

    const TIMER_InitCC_TypeDef timer_cc_data =
    {
      .eventCtrl  = timerEventRising,
      .edge       = timerEdgeRising,
      .prsSel     = timerPRSSELCh0,         /* Prs channel select channel 5*/
      .cufoa      = timerOutputActionNone,  /* No action on counter underflow */
      .cofoa      = timerOutputActionNone,  /* No action on counter overflow */
      .cmoa       = timerOutputActionNone,  /* No action on counter match */
      .mode       = timerCCModeCapture,     /* CC channel mode capture */
      .filter     = false,                  /* No filter */
      .prsInput   = true,                   /* CC channel PRS input */
      .coist      = false,                  /* Comparator output initial state */
      .outInvert  = false,
    };

    TIMER_InitCC(TIMER1, 1, &timer_cc_data);

    const TIMER_Init_TypeDef timer_init_data =
    {
        .enable     = false,
        .debugRun   = false,
        .prescale   = timerPrescale1,
        .clkSel     = timerClkSelCC1,
        .fallAction = timerInputActionNone,
        .riseAction = timerInputActionNone,
        .mode       = timerModeUp,
        .dmaClrAct  = false,
        .quadModeX4 = false,
        .oneShot    = false,
        .sync       = true,
    };

    TIMER_Init(TIMER1, &timer_init_data);

    // Set PRS to fire on pin change
    PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_GPIOL,
            PRS_CH_CTRL_SIGSEL_GPIOPIN0, prsEdgePos);

    // Turn everything off again
    CMU_ClockEnable(cmuClock_TIMER1, false);
    CMU_ClockEnable(cmuClock_PRS, false);

    NVIC_EnableIRQ(GPIO_EVEN_IRQn);

}

/**
 * Configure a 1kHz timer to count pulses with, but don't start it
 */
void pin_timer_init(void)
{
    // Power the timer up
    CMU_ClockEnable(cmuClock_TIMER0, true);

    // Configure 100us capture-compare
    const TIMER_InitCC_TypeDef timer_cc_data =
    {
      .cufoa      = timerOutputActionNone,  /* No action on counter underflow */
      .cofoa      = timerOutputActionNone,  /* No action on counter overflow */
      .cmoa       = timerOutputActionNone,  /* No action on counter match */
      .mode       = timerCCModeCompare,     /* CC channel mode capture */
      .filter     = false,                  /* No filter */
      .prsInput   = false,                  /* CC channel PRS input */
      .coist      = false,                  /* Comparator output initial state */
      .outInvert  = false,
    };

    TIMER_InitCC(TIMER0, 0, &timer_cc_data);

    const TIMER_Init_TypeDef timerInit =
    {
        .clkSel = timerClkSelHFPerClk,
        .debugRun = false,
        .dmaClrAct = false,
        .enable = false,
        .fallAction = timerInputActionStop,
        .mode = timerModeUp,
        .oneShot = false,
        .prescale = timerPrescale2,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_IntEnable(TIMER0, TIMER_IF_OF);
    NVIC_EnableIRQ(TIMER0_IRQn);

    TIMER_Init(TIMER0, &timerInit);

    // Wrap around is about 1kHz
    timer_1ms_ticks = CMU_ClockFreqGet(cmuClock_TIMER0)/(2 * 1000);
    timer_2ms_ticks = CMU_ClockFreqGet(cmuClock_TIMER0)/(2 * 500);
    TIMER_TopSet(TIMER0, timer_1ms_ticks);

    setup_detect_algorithm(timer_1ms_ticks, timer_2ms_ticks);

    // Also configure compare to fire after about 100us
    TIMER_CompareSet(TIMER0, 0, timer_1ms_ticks/10);

    // Power it down until we need it
    CMU_ClockEnable(cmuClock_TIMER0, false);
}

/**
 * Handle an incoming rising edge by triggering pulse count
 */
void GPIO_EVEN_IRQHandler(void)
{
    initial_detect();
    GPIO_IntClear(1);
}

/**
 * Handle the timer timeout by resetting top value and sorting the state machine
 */
void TIMER0_IRQHandler(void)
{
    if (TIMER_IntGet(TIMER0) & TIMER_IF_CC0)
    {
        // Run algorithm to check we're getting a valid call
        short_timeout();

        // Clear interrupt flag
        TIMER_IntClear(TIMER0, TIMER_IF_CC0);
    }
    else
    {
        // Run algorithm to reset timer and update state machine
        full_timeout();

        // Clear interrupt flag
        TIMER_IntClear(TIMER0, TIMER_IF_OF);
    }
}

/**
 * Select which sleep mode to enter by state machine
 */
void sleep_handler(void)
{
    if (timer_active == true)
    {
        EMU_EnterEM1();
    }
    else
    {
        EMU_EnterEM2(false);
    }
}
