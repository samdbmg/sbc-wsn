/**
 * Artificial cricket main file
 *
 */

/* Standard libraries */
#include <stdio.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "timer_config.h"
#include "random_adjust.h"
#include "user_switch.h"
#include "serial_interface.h"

// Set this to zero to disable random adjust
#define RANDOM_ENABLE 1

// Set this to zero to only call on button push
#define CALL_TIMER_ENABLE 1

// Initial number of clicks per call
#define INITIAL_CLICK_COUNT 7

// Generate a female response after this many (set to zero to disable)
#define FEMALE_RESPONSE_PERIOD 1
#define FEMALE_RESPONSE_DELAY_MS 25

// Set to zero to disable serial data output
#define SERIAL_OUT 1

// Do we want to be able to debug the CPU with the ST-Link?
#define DEBUG_ENABLE 1

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
static uint8_t g_calls_sent = 0;

// Initial call timer value
static const uint32_t g_initial_call_timer = 1000;

static int8_t g_female_wait = FEMALE_RESPONSE_PERIOD - 1;

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

        // Have we generated enough calls (or run once for female response)
        if ((g_clicks_progress >= g_clicks_total) || (g_female_wait == -2))
        {
            // Start the call timer
            TIM_Cmd(TIM2, ENABLE);

            // Disable the mark/space timer and wait until call timer triggers
            TIM_Cmd(TIM4, DISABLE);

            // Turn the LED off
            GPIO_WriteBit(GPIOD, GPIO_Pin_15, Bit_RESET);
        }

        // If we just did a female response, reset to do male calls
        if (g_female_wait == -2)
        {
            g_female_wait = FEMALE_RESPONSE_PERIOD - 1;
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
    // Simple debounce
    for (volatile uint16_t i = 0; i < 0xFFFF; i++)
    {

    }

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0))
    {
        // Trigger a call immediately
        generate_call();

        EXTI_ClearITPendingBit(EXTI_Line0);
    }
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

    // Stop the call timer
    TIM_Cmd(TIM2, DISABLE);

    // Should female responses be considered?
    if (FEMALE_RESPONSE_PERIOD > 0)
    {
        // Should we be generating a female call next time round?
        if (g_female_wait == 0)
        {
            call_timer_female(true);

            // Set the flag to mark we're waiting on a female call
            g_female_wait = -1;
        }
        else if (g_female_wait == -1)
        {
            // This call should be a female response, so set the timer back
            call_timer_female(false);

            // Also mark for the benefit of the mark/space timer than we want
            // only one click
            g_female_wait = -2;
        }
        else
        {
            g_female_wait--;

            g_calls_sent++;
        }
    }

    // Randomly adjust all the parameters
    if (RANDOM_ENABLE && g_female_wait != -1)
    {
        // Overall call timer
        TIM2->ARR = (uint32_t)random_percentage_adjust(10, 25, (int32_t)TIM2->ARR,
                (int32_t)g_initial_call_timer*1000);

        // Number of clicks
        g_clicks_total = (uint8_t)random_percentage_adjust(50, 20,
                (int32_t)g_clicks_total, INITIAL_CLICK_COUNT);

        // Mark/space ratio and call lengths
        TIM4->ARR = (uint32_t)random_percentage_adjust(2,5,
                (int32_t)TIM4->ARR, MARKSPACE_TICKS);
        TIM4->CCR1 = (uint32_t)random_percentage_adjust(2, 5,
                (int32_t)TIM4->CCR1, MARKSPACE_COMPARE_VALUE);

    }

#if SERIAL_OUT
    serial_print_char((char)g_calls_sent);
#endif
}

/**
 * Main function. Sets up pins and PRNG, then continuously outputs clicks
 *
 * @return Never returns.
 */
void main(void)
{
#if DEBUG_ENABLE
    // To prevent random debugger disconnects, set this
    DBGMCU_Config(DBGMCU_SLEEP | DBGMCU_STOP | DBGMCU_STANDBY, ENABLE);
#endif

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

#if SERIAL_OUT
    serial_init();
#endif

#if CALL_TIMER_ENABLE
    call_timer_setup(g_initial_call_timer, FEMALE_RESPONSE_DELAY_MS);
#endif

    // Go to sleep. Interrupts will do the rest
    while (1)
    {
        __WFI();
    }
}
