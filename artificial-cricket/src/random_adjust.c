/*
 * Functions to generate random variation using the on-chip RNG
 */

#include <stdlib.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application specific headers */
#include "random_adjust.h"

/**
 * Initialise and enable the RNG
 */
void random_init(void)
{
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
}

/**
 * Adjust a value randomly within an approximate percentage range
 * Adjusts by +/-size, but stays within max of initial value
 *
 * @param size    Percentage to vary the number by
 * @param max     Maximum percentage variation from initial value
 * @param current The current value
 * @param initial The original setpoint before successive randomness
 * @return        New value. May be no change if max was exceeded
 */
int32_t random_percentage_adjust(uint8_t size, uint8_t max, int32_t current,
        int32_t initial)
{
    int32_t boundary = current / (100 / size);
    int32_t factor = (int32_t)RNG_GetRandomNumber() % boundary;
    factor -= boundary/2;

    int32_t new = current += factor;

    int32_t percentage_change = (100 * abs(new - initial))/initial;

    // Prevent an overly large swing in either direction
    if (percentage_change < max)
    {
        return new;
    }
    else
    {
        return initial;
    }
}
