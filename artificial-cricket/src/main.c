/**
 * Artificial cricket main file
 *
 */
#include <stdio.h>

#include "stm32f4xx.h"

#include "timer_config.h"


// Counter for number of clicks to generate each time, and number so far
uint8_t g_clicks_total;
uint8_t g_clicks_progress;

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
            //TIM_CMD(TIM4, DISABLE);
        }

        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}

/**
 * Main function. Sets up pins and PRNG, then continuously outputs clicks
 *
 * @return Never returns.
 */
void main(void)
{
    RCC_ClocksTypeDef RCC_Clocks;

    // Use SysTick as reference for the timer
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

    // Configure the assorted timers
    pwm_timer_setup();
    markspace_timer_setup();
    //call_timer_setup();

    // Enable the randomness generator
    //random_adjust_enable();

    // Configure the switch input
    //switch_setup();

    // Do nothing. Everything proceeds in interrupts
    while (1)
    {
        // Add your code here.
    }
}
