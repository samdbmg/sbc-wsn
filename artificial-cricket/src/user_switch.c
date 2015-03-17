/**
 * Function to activate the user switch and interrupts to use it
 *
 */

/* Standard libraries */
#include <stdio.h>

/* Board support headers */
#include "stm32f4xx.h"
#include "stm32f4xx_GPIO.h"

/* Application-specific headers */
#include "user_switch.h"

/**
 * Initialise the user switch and interrupt line
 */
void user_switch_init(void)
{
    // Configure the User switch to trigger a call
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef switch_initstruct;
    switch_initstruct.GPIO_Pin = GPIO_Pin_0;
    switch_initstruct.GPIO_Mode = GPIO_Mode_IN;
    switch_initstruct.GPIO_OType = GPIO_OType_PP;
    switch_initstruct.GPIO_PuPd = GPIO_PuPd_DOWN;
    switch_initstruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &switch_initstruct);

    // Enable external interrupts
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    EXTI_InitTypeDef exti_def;
    exti_def.EXTI_Line = EXTI_Line0;
    exti_def.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_def.EXTI_Trigger = EXTI_Trigger_Rising;
    exti_def.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_def);

    // Connect EXTI bits
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

    // Enable interrupt and set priority
    NVIC_InitTypeDef nvicStructure;
    nvicStructure.NVIC_IRQChannel = EXTI0_IRQn;
    nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
    nvicStructure.NVIC_IRQChannelSubPriority = 1;
    nvicStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvicStructure);
}
