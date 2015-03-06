/*
 * Artificial cricket control timers setup
 *
 */


#include <stdio.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

/* Application specific headers */
#include "timer_config.h"

static uint32_t g_call_period_ms;
static uint32_t g_female_period_ms;

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

    // Timer base configuration (TIM3, 16 bit GP timer)
    TIM_TimeBaseInitTypeDef TIM_initstruct;
    TIM_initstruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_initstruct.TIM_Prescaler = 0;
    TIM_initstruct.TIM_Period = PWM_TICKS;
    TIM_initstruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_initstruct);

    // PWM Channel 1 configuration
    TIM_OCInitTypeDef TIM_ocstruct;
    TIM_ocstruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_ocstruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_ocstruct.TIM_Pulse = PWM_COMPARE_VALUE;
    TIM_ocstruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_ocstruct);

    // Activate TIM3 preloads
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);

    // Timer will be enabled in ISR for TIM4 (mark-space) compare match
}

/**
 * Configure the mark/space timer to generate 1ms calls with 2ms spacing
 */
void markspace_timer_setup(void)
{
    // Start up timer clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    // Timer base configuration (TIM4, 16 bit GP timer)
    TIM_TimeBaseInitTypeDef TIM_initstruct;
    TIM_initstruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_initstruct.TIM_Prescaler = MARKSPACE_PSC-1;
    TIM_initstruct.TIM_Period = MARKSPACE_TICKS;
    TIM_initstruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_initstruct);

    // Output Compare configuration
    TIM_OCInitTypeDef TIM_ocstruct;
    TIM_ocstruct.TIM_OCMode = TIM_OCMode_Timing;
    TIM_ocstruct.TIM_OutputState = TIM_OutputState_Disable;
    TIM_ocstruct.TIM_Pulse = MARKSPACE_COMPARE_VALUE;
    TIM_OC1Init(TIM4, &TIM_ocstruct);

    // Activate TIM4 preloads
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);

    // Enable the overflow interrupt
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    // Enable the compare match interrupt
    TIM_ITConfig(TIM4, TIM_IT_CC1, ENABLE);

    // Enable the timer's interrupts in the interrupt controller
    NVIC_InitTypeDef NVIC_compare_initstruct;
    NVIC_compare_initstruct.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_compare_initstruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_compare_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_compare_initstruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_compare_initstruct);

    // Timer will be enabled in ISR for call timer (TIM2) overflow
}

/**
 * Configure the timer to generate calls
 *
 * @param call_period_ms Time in milliseconds between call generations.
 *                       Overflows after an hour
 */
void call_timer_setup(uint32_t call_period_ms, uint32_t female_period_ms)
{
    g_call_period_ms = call_period_ms;
    g_female_period_ms = female_period_ms;

    // Start up timer clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // Timer base configuration (TIM2, 32 bit GP timer)
    TIM_TimeBaseInitTypeDef TIM_initstruct;
    TIM_initstruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_initstruct.TIM_Prescaler = CALL_TIMER_PSC-1;
    TIM_initstruct.TIM_Period = call_period_ms * 1000;
    TIM_initstruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_initstruct);

    TIM_ARRPreloadConfig(TIM4, ENABLE);

    // Enable the overflow interrupt
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // Enable the timer's interrupts in the interrupt controller
    NVIC_InitTypeDef NVIC_compare_initstruct;
    NVIC_compare_initstruct.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_compare_initstruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_compare_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_compare_initstruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_compare_initstruct);

    // Enable the timer
    TIM_Cmd(TIM2, ENABLE);
}

/**
 * Reconfigure the call timer either for a female response or a standard call
 * @param enable Activate female response mode if true
 */
void call_timer_female(bool enable)
{
    if (enable)
    {
        TIM2->ARR = g_female_period_ms * 1000;
    }
    else
    {
        TIM2->ARR = g_call_period_ms * 1000;
    }
}
