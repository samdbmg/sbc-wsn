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

bool timer_active = false;
bool last_step = false;

uint32_t timer_1ms_ticks;
uint32_t timer_2ms_ticks;

uint32_t high_count;
uint64_t low_count;

/**
 * Call the init functions below
 */
void pin_mode_init(void)
{
    pinchange_init();
    pin_timer_init();
}

/**
 * Set up a pin to increment a counter when the state changes
 */
void pinchange_init(void)
{
    // Activate PCNT clock
    CMU_ClockEnable(cmuClock_PCNT0, true);

    // Configure PC0 as an input pin
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);

    PCNT_Init_TypeDef pcnt_init_data;
    pcnt_init_data.mode = pcntModeOvsSingle;
    pcnt_init_data.counter = 0;
    pcnt_init_data.filter = true;

    PCNT_Enable(PCNT0, false);
    PCNT_Init(PCNT0, &pcnt_init_data);

    // Use location 2 (PC0) as source
    PCNT0->ROUTE = PCNT_ROUTE_LOCATION_LOC2;

    // Clear and stop it, then kill the clock (we don't need it yet)
    PCNT_CounterReset(PCNT0);
    CMU_ClockEnable(cmuClock_PCNT0, false);

    // Configure GPIO PC0 to fire an interrupt on a rising edge
    NVIC_EnableIRQ(GPIO_EVEN_IRQn);
    GPIO_IntConfig(gpioPortC, 0, true, false, true);
}

/**
 * Handle an incoming rising edge by triggering pulse count
 */
void GPIO_EVEN_IRQHandler(void)
{
    NVIC_DisableIRQ(GPIO_EVEN_IRQn);
    GPIO_IntClear(1);

    CMU_ClockEnable(cmuClock_PCNT0, true);
    CMU_ClockEnable(cmuClock_TIMER0, true);

    timer_active = true;

    TIMER_Enable(TIMER0, true);
}

/**
 * Handle an RTC timeout by setting some values
 */
void rtc_handler(void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        USART_SpiTransfer(USART1, high_count >> (i*8));
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        USART_SpiTransfer(USART1, low_count >> (i*8));
    }
}

/**
 * Configure a 1kHz timer to count pulses with, but don't start it
 */
void pin_timer_init(void)
{
    // Power the timer up
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
        .prescale = timerPrescale2,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_IntEnable(TIMER0, TIMER_IF_OF);
    NVIC_EnableIRQ(TIMER0_IRQn);

    TIMER_Init(TIMER0, &timerInit);

    // Wrap around is about 1kHz
    timer_1ms_ticks = CMU_ClockFreqGet(cmuClock_TIMER0)/1000;
    timer_2ms_ticks = CMU_ClockFreqGet(cmuClock_TIMER0)/500;
    TIMER_TopSet(TIMER0, timer_1ms_ticks);

    // Power it down until we need it
    CMU_ClockEnable(cmuClock_TIMER0, false);
}

/**
 * Handle the timer timeout by resetting top value and sorting the state machine
 */
void TIMER0_IRQHandler(void)
{
    // Clear interrupt flag
    TIMER_IntClear(TIMER0, TIMER_IF_OF);

    if (last_step == false)
    {
        // Reconfigure the timer to count for 2ms
        TIMER_TopSet(TIMER0, timer_2ms_ticks);

        // Save the counter value and reset it
        high_count += PCNT_CounterGet(PCNT0);
        PCNT_CounterReset(PCNT0);

        // Mark that next time we should stop counting
        last_step = true;
    }
    else
    {
        // Reconfigure the timer back to 1ms and turn it off
        TIMER_TopSet(TIMER0, timer_1ms_ticks);
        TIMER_Enable(TIMER0, false);
        CMU_ClockEnable(cmuClock_TIMER0, false);

        // Save the count value and turn it off
        low_count += PCNT_CounterGet(0);
        PCNT_CounterReset(PCNT0);
        PCNT_Enable(PCNT0, false);
        CMU_ClockEnable(cmuClock_PCNT0, false);

        // Reset the state machine
        last_step = false;
        timer_active = false;

        // Re-enable the pin change interrupt
        NVIC_EnableIRQ(GPIO_EVEN_IRQn);
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
