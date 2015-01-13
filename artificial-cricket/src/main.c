/**
 * Artificial cricket main file
 *
 */
#include <stdio.h>

#include "stm32f4xx.h"

#include "timer_config.h"

/* Declare functions used only in this file */
// Mark/space timer IRQ
void TIM4_IRQHandler(void);
// Call timer IRQ
void TIM2_IRQHandler(void);

// Counter for number of clicks to generate each time, and number so far
static uint8_t g_clicks_total = 5;
static volatile uint8_t g_clicks_progress;

/**
 * Handle mark space timer overflow and compare match to turn pulse on and off
 */
void TIM4_IRQHandler(void)
{
    if (SET == TIM_GetITStatus(TIM4, TIM_IT_CC1))
    {
        // Timer compare interrupt fired. Turn PWM on
        TIM_Cmd(TIM3, ENABLE);

        TIM_ClearITPendingBit(TIM4, TIM_IT_CC1);
    }
    else if (SET == TIM_GetITStatus(TIM4, TIM_IT_Update))
    {
        // Only other enabled interrupt is overflow, so overflow must have
        // fired. PWM off.
        TIM_Cmd(TIM3, DISABLE);

        g_clicks_progress++;

        // Have we generated enough calls
        if (g_clicks_progress > g_clicks_total)
        {
            // Disable the mark/space timer and wait until call timer triggers
            TIM_Cmd(TIM4, DISABLE);

            // Turn the LED off
            GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_RESET);
        }

        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}

/**
 * Handle call timer overflow by generating a cricket call
 */
void TIM2_IRQHandler(void)
{
    if (SET == TIM_GetITStatus(TIM2, TIM_IT_Update))
    {
        // Turn the LED on
        GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_SET);

        // Reset the click progress count to zero
        g_clicks_progress = 0;

        // Enable the mark/space timer
        TIM_Cmd(TIM4, ENABLE);

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

/**
 * Main function. Sets up pins and PRNG, then continuously outputs clicks
 *
 * @return Never returns.
 */
void main(void)
{
    // Configure a board LED to illuminate while calling
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_InitTypeDef GPIO_initstruct;
    GPIO_initstruct.GPIO_Pin = GPIO_Pin_15;
    GPIO_initstruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_initstruct.GPIO_OType = GPIO_OType_PP;
    GPIO_initstruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_initstruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOD, &GPIO_initstruct);

    GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_RESET);

    // Configure the assorted timers
    pwm_timer_setup();
    markspace_timer_setup();
    call_timer_setup(1000);

    // Enable the randomness generator
    //random_adjust_enable();

    // Configure the switch input
    //switch_setup();

    // Go to sleep. Interrupts will do the rest
    __WFI();
}
