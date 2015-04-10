/**
 * Miscellaneous helper functions that don't live anywhere else
 */

/* Standard libraries */
#include <base_misc.h>
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "base_misc.h"
#include "power_management.h"

static volatile bool _delay_active = false;

/**
 * Set up a timer to use for general purpose delays
 */
void misc_delay_init(void)
{
    // Start up timer clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    // Timer base configuration (TIM5, 32 bit GP timer)
    TIM_TimeBaseInitTypeDef timerInit =
    {
        .TIM_ClockDivision = TIM_CKD_DIV1,
        .TIM_Prescaler = 32000, // Corresponds to a 1ms tick period
        .TIM_Period = 1000,
        .TIM_CounterMode = TIM_CounterMode_Up,
    };

    TIM_TimeBaseInit(TIM5, &timerInit);

    TIM_ARRPreloadConfig(TIM5, DISABLE);

    // Enable the overflow interrupt
    TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

    // Enable the timer's interrupts in the interrupt controller
    NVIC_InitTypeDef nvicInit =
    {
            .NVIC_IRQChannel = TIM5_IRQn,
            .NVIC_IRQChannelPreemptionPriority = 0,
            .NVIC_IRQChannelSubPriority = 1,
            .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvicInit);

    // Power down the timer until needed
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);
}

/**
 * Delay execution for a period of time (will drop to sleep and interrupts
 * will not be suppressed)
 *
 * @param ms       Time in ms to delay for. Do not exceed 49 days
 * @param block    Block execution in sleep mode if set to true
 */
void misc_delay(uint32_t ms, bool block)
{
    // Calculate ticks
    uint32_t delay_ticks = get_ticks_from_ms(ms, 32000);

    // Power up the timer
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    // Keep clocks on
    power_set_minimum(PWR_DELAY, PWR_SLEEP);

    // Mark delay active
    _delay_active = true;

    // Set timer and start
    TIM5->ARR = delay_ticks;
    TIM_Cmd(TIM5, ENABLE);

    // Wait for timeout
    if (block)
    {
        while (_delay_active)
        {
            power_sleep();
        }
    }
}

/**
 * Check if a delay is currently active (useful in non-blocking mode)
 * @return True if a delay is active
 */
bool misc_delay_active(void)
{
    return _delay_active;
}

/**
 * Handle timer ending by stopping the delay
 */
void TIM5_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM5, TIM_IT_Update))
    {
        // Timeout reached, clear flag, shut down the timer
        _delay_active = false;

        TIM_Cmd(TIM5, DISABLE);
        TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);

        power_set_minimum(PWR_DELAY, PWR_CLOCKSTOP);
    }
}

void misc_ringbuffer_write(misc_ringbuf_t *buffer, uint8_t* pdata, uint16_t bytes)
{
    while (bytes-- > 0)
    {
        *(buffer->write_pointer)++ = *pdata++;

        if (buffer->write_pointer >= (buffer->data_pointer + buffer->length))
        {
            buffer->write_pointer = buffer->data_pointer;
        }

        if (buffer->write_pointer == buffer->read_pointer)
        {
            buffer->read_pointer++;

            if (buffer->read_pointer >= (buffer->data_pointer + buffer->length))
            {
                buffer->read_pointer = buffer->data_pointer;
            }
        }
    }
}

uint16_t misc_ringbuffer_read(misc_ringbuf_t *buffer, uint8_t* pdata, uint16_t bytes)
{
    uint16_t byte_count = 0;

    for (; byte_count < bytes; byte_count++)
    {
        *pdata++ = *(buffer->read_pointer)++;

        if (buffer->read_pointer >= (buffer->data_pointer + buffer->length))
        {
            buffer->read_pointer = buffer->data_pointer;
        }

        if (buffer->read_pointer == buffer->write_pointer)
        {
            // Buffer is empty, return result
            break;
        }
    }

    return byte_count;
}

void misc_ringbuffer_clear(misc_ringbuf_t *buffer)
{
    buffer->read_pointer = buffer->write_pointer;
}
