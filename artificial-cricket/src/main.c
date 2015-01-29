/*
 * Artificial cricket main file
 *
 */
#include <stdio.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application specific headers */
#include "timer_config.h"
#include "random_adjust.h"
#include "user_switch.h"

// Set this to zero to disable random adjust
#define RANDOM_ENABLE 1

// Initial number of clicks per call
#define INITIAL_CLICK_COUNT 7

/* Declare functions used only in this file */
// Mark/space timer IRQ
void TIM4_IRQHandler(void);
// Call timer IRQ
void TIM2_IRQHandler(void);
// Switch IRQ
void EXTI0_IRQHandler(void);

static void generate_call(void);

// Counter for number of clicks to generate each time, and number so far
static uint8_t g_clicks_total = INITIAL_CLICK_COUNT;
static volatile uint8_t g_clicks_progress;

// Initial call timer value
static const uint32_t g_initial_call_timer = 5000;

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
        if (g_clicks_progress >= g_clicks_total)
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
        generate_call();

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

void EXTI0_IRQHandler(void)
{
    // Trigger a call immediately
    generate_call();

    EXTI_ClearITPendingBit(EXTI_Line0);
}

/**
 * Generate a call and do random updates
 */
static void generate_call(void)
{
    // Turn the LED on
    GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_SET);

    // Reset the click progress count to zero
    g_clicks_progress = 0;

    // Enable the mark/space timer
    TIM_Cmd(TIM4, ENABLE);

    // Randomly adjust all the parameters
    if (RANDOM_ENABLE)
    {
        // Overall call timer
        TIM2->ARR = (uint32_t)random_percentage_adjust(10, 25, (int32_t)TIM2->ARR,
                (int32_t)g_initial_call_timer*1000);

        // Number of clicks
        g_clicks_total = (uint8_t)random_percentage_adjust(50, 30,
                (int32_t)g_clicks_total, INITIAL_CLICK_COUNT);

        // Mark/space ratio and call lengths
        TIM4->ARR = (uint32_t)random_percentage_adjust(2,5,
                (int32_t)TIM4->ARR, MARKSPACE_TICKS);
        TIM4->CCR1 = (uint32_t)random_percentage_adjust(2, 5,
                (int32_t)TIM4->CCR1, MARKSPACE_COMPARE_VALUE);

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

    GPIO_InitTypeDef LED_initstruct;
    LED_initstruct.GPIO_Pin = GPIO_Pin_15;
    LED_initstruct.GPIO_Mode = GPIO_Mode_OUT;
    LED_initstruct.GPIO_OType = GPIO_OType_PP;
    LED_initstruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    LED_initstruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOD, &LED_initstruct);

    GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_RESET);

    // Enable the USER switch to trigger a call
    user_switch_init();

    // Enable the randomness generator
    random_init();

    // Configure the assorted timers
    pwm_timer_setup();
    markspace_timer_setup();
    call_timer_setup(g_initial_call_timer);

    // Go to sleep. Interrupts will do the rest
    __WFI();
}
