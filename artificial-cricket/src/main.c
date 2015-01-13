/**
 * Artificial cricket main file
 *
 */
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

void pwm_timer_setup(void);

/**
 * Configure the PWM timer to generate 40kHz
 */
void pwm_timer_setup(void)
{
    // Start up timer and GPIO clocks
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // Configure output pin
    GPIO_InitTypeDef GPIO_initstruct;
    GPIO_initstruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_initstruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_initstruct.GPIO_OType = GPIO_OType_PP;
    GPIO_initstruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_initstruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &GPIO_initstruct);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM3);

    // Timer base configuration
    TIM_TimeBaseInitTypeDef TIM_initstruct;
    TIM_initstruct.TIM_ClockDivision = 0;
    TIM_initstruct.TIM_Prescaler = 0;
    TIM_initstruct.TIM_Period = 1800;
    TIM_initstruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_initstruct);

    // PWM Channel 1 configuration
    TIM_OCInitTypeDef TIM_ocstruct;
    TIM_ocstruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_ocstruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_ocstruct.TIM_Pulse = TIM_initstruct.TIM_Period / 4;
    TIM_ocstruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_ocstruct);

    // Activate TIM3 preloads
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);

    // Run timer
    TIM_Cmd(TIM3, ENABLE);

    // Set 50% duty
    TIM_SetCompare1(TIM3, 900);
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
    //markspace_timer_setup();
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
