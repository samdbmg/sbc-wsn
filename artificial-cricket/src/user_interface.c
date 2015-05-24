/**
 * Function to activate the user switch and interrupts to use it
 *
 */

/* Standard libraries */
#include <stdio.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"
#include "stm32f4xx_GPIO.h"
#include "stm32f4xx_ADC.h"

/* Application-specific headers */
#include "user_interface.h"
#include "timer_config.h"

volatile uint16_t adc_readings[2];

extern uint8_t g_female_wait;
extern uint32_t g_call_period_ms;
extern uint8_t g_female_period;

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

/**
 * Initialise the ADC channels to read call time and female response freq
 */
void adc_init(void)
{
    // ADC GPIO Setup
    GPIO_InitTypeDef gpioInit =
    {
            .GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5,
            .GPIO_Mode = GPIO_Mode_AN,
            .GPIO_PuPd = GPIO_PuPd_NOPULL
    };
    GPIO_Init(GPIOA, &gpioInit);

    // Power up the DMA
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // Configure DMA channel
    DMA_DeInit(DMA2_Stream0);
    DMA_InitTypeDef dmaInit =
    {
            .DMA_Channel = DMA_Channel_0,
            .DMA_PeripheralBaseAddr = (uint32_t)&(ADC1->DR),
            .DMA_Memory0BaseAddr = (uint32_t)adc_readings,
            .DMA_DIR = DMA_DIR_PeripheralToMemory,
            .DMA_BufferSize = 2,
            .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
            .DMA_MemoryInc = DMA_MemoryInc_Enable,
            .DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord,
            .DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord,
            .DMA_Mode = DMA_Mode_Circular,
            .DMA_Priority = DMA_Priority_High,
            .DMA_FIFOMode = DMA_FIFOMode_Disable,
            .DMA_FIFOThreshold = DMA_FIFOStatus_HalfFull,
            .DMA_MemoryBurst = DMA_MemoryBurst_Single,
            .DMA_PeripheralBurst = DMA_PeripheralBurst_Single
    };
    DMA_Init(DMA2_Stream0, &dmaInit);
    DMA_Cmd(DMA2_Stream0, ENABLE);

    // Power up the ADC
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    ADC_CommonInitTypeDef adcCommonInit =
    {
            .ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled,
            .ADC_Mode = ADC_Mode_Independent,
            .ADC_Prescaler = ADC_Prescaler_Div2,
            .ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles
    };

    ADC_CommonInit(&adcCommonInit);

    ADC_InitTypeDef adcInit =
    {
            .ADC_Resolution = ADC_Resolution_12b,
            .ADC_ContinuousConvMode = DISABLE,
            .ADC_DataAlign = ADC_DataAlign_Right,
            .ADC_ScanConvMode = ENABLE,
            .ADC_NbrOfConversion = 2,
            .ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None,
            .ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1
    };

    ADC_Init(ADC1, &adcInit);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_112Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 2, ADC_SampleTime_112Cycles);

    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    // Configure a timer to run conversions
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    // Timer base configuration (TIM5, 32 bit GP timer)
    TIM_TimeBaseInitTypeDef TIM_initstruct;
    TIM_initstruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_initstruct.TIM_Prescaler = CALL_TIMER_PSC-1;
    TIM_initstruct.TIM_Period = 1000;
    TIM_initstruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM5, &TIM_initstruct);

    TIM_ARRPreloadConfig(TIM5, ENABLE);

    // Enable the overflow interrupt
    TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

    // Enable the timer's interrupts in the interrupt controller
    NVIC_InitTypeDef NVIC_compare_initstruct;
    NVIC_compare_initstruct.NVIC_IRQChannel = TIM5_IRQn;
    NVIC_compare_initstruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_compare_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_compare_initstruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_compare_initstruct);

    // Enable the timer
    TIM_Cmd(TIM5, ENABLE);

    // Convert now
    ADC_SoftwareStartConv(ADC1);
}

void TIM5_IRQHandler(void)
{
    if (SET == TIM_GetITStatus(TIM5, TIM_IT_Update))
    {
        // Start next
        ADC_SoftwareStartConv(ADC1);

        // Handle call timer if not currently doing female
        if (g_female_wait >= -1)
        {
            if (adc_readings[0] > ZERO_ABOVE_THRESHOLD)
            {
                // We're above the threshold, consider this zero, stop timer
                TIM_Cmd(TIM2, DISABLE);
                TIM2->ARR = 100;
                TIM_SetCounter(TIM2, 0);
            }
            else
            {
                // Scale the reading into ms
                g_call_period_ms = 500 + (adc_readings[0] * (60000/4096));
                TIM2->ARR = g_call_period_ms * 1000;

                if (TIM_GetCounter(TIM2) > TIM2->ARR)
                {
                    TIM_SetCounter(TIM2, TIM2->ARR - 10);
                }

                TIM_Cmd(TIM2, ENABLE);
            }
        }

        // Next female reponse
        if (adc_readings[1] > ZERO_ABOVE_THRESHOLD)
        {
            // We're above the threshold, consider this zero
            g_female_period = 0;
        }
        else
        {
            // Scale the reading to be 1-100
            g_female_period = 1 + (adc_readings[1] * 100) / 4096;
        }


        TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
    }
}
