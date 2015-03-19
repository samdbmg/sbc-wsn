/**
 * SPI interface driver for RFM69W radio module
 * This code exposes functions for radio_control.c to use with the STM32 MCU
 */

// Standard libraries
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "radio_spi.h"
#include "misc.h"
#include "radio_control.h"
#include "power_management.h"

void EXTI8_IRQHandler(void);

// Type of interrupt currently being waited on
static volatile uint8_t interrupt_state = RADIO_INT_NONE;

/**
 * Configure the SPI peripheral and pins to talk to the radio
 */
void radio_spi_init(void)
{
    // Power up SPI module clocks
    radio_spi_powerstate(true);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // Set up SPI pins
    GPIO_InitTypeDef gpioInit =
    {
            .GPIO_Mode = GPIO_Mode_AF,
            .GPIO_OType = GPIO_OType_PP,
            .GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7,
            .GPIO_PuPd = GPIO_PuPd_DOWN,
            .GPIO_Speed = GPIO_Speed_50MHz
    };
    GPIO_Init(GPIOA, &gpioInit);

    // Configure alternate function mappings to SPI peripheral
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1); // SCK
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1); // MISO
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1); // MOSI

    // Set up NSS chip select pin as actual GPIO
    gpioInit.GPIO_Mode = GPIO_Mode_OUT;
    gpioInit.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &gpioInit);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    // Configure the SPI peripheral itself
    SPI_InitTypeDef spiInit =
    {
            .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64,
            .SPI_CPHA = SPI_CPHA_1Edge,
            .SPI_CPOL = SPI_CPOL_Low,
            .SPI_CRCPolynomial = 7,
            .SPI_DataSize = SPI_DataSize_8b,
            .SPI_Direction = SPI_Direction_2Lines_FullDuplex,
            .SPI_FirstBit = SPI_FirstBit_MSB,
            .SPI_Mode = SPI_Mode_Master,
            .SPI_NSS = SPI_NSS_Soft
    };
    SPI_Init(SPI1, &spiInit);

    // Turn SPI on
    SPI_Cmd(SPI1, ENABLE);

    // Configure the interrupt pin
    GPIO_InitTypeDef gpioIntInit =
    {
            .GPIO_Mode = GPIO_Mode_IN,
            .GPIO_Pin = GPIO_Pin_8,
            .GPIO_PuPd = GPIO_PuPd_NOPULL,
            .GPIO_Speed = GPIO_Speed_2MHz
    };
    GPIO_Init(GPIOA, &gpioIntInit);

    // Enable external interrupts
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    EXTI_InitTypeDef extiInit =
    {
            .EXTI_Line = EXTI_Line8,
            .EXTI_LineCmd = ENABLE,
            .EXTI_Mode = EXTI_Mode_Interrupt,
            .EXTI_Trigger = EXTI_Trigger_Rising
    };
    EXTI_Init(&extiInit);

    // Connect EXTI bits
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource8);

    // Enable interrupt and set priority
    NVIC_InitTypeDef nvicStructure;
    nvicStructure.NVIC_IRQChannel = EXTI0_IRQn;
    nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
    nvicStructure.NVIC_IRQChannelSubPriority = 1;
    nvicStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvicStructure);

}

/**
 * Enable or disable clock to the SPI module
 * @param state True to power up, false to power down
 */
void radio_spi_powerstate(bool state)
{
    //RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, (state ? ENABLE : DISABLE));
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, (state ? ENABLE : DISABLE));
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, (state ? ENABLE : DISABLE));
}

/**
 * Send and receive single bytes of data
 *
 * @param send_data A byte of data to send, set to zero for a read
 * @return          Received byte
 */
uint8_t radio_spi_transfer(uint8_t send_data)
{
    SPI1->DR = send_data;

    /* Wait for transmission to complete */
    while ((SPI1->SR & SPI_SR_TXE) == 0);
    /* Wait for received data to complete */
    while ((SPI1->SR & SPI_SR_RXNE) == 0);
    /* Wait for SPI to be ready */
    while (SPI1->SR & SPI_SR_BSY);


    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

/**
 * Assert or release the NSS line
 *
 * @param select Set to true to assert NSS low, false to release
 */
void radio_spi_select(bool select)
{
    if (select)
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_4);
    }
    else
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_4);
    }
}

/**
 * Wait until the interrupt pin asserts that transmit is complete.
 */
void radio_spi_transmitwait(void)
{
    // Sleep until the flag is cleared by the interrupt routine
    while (interrupt_state != RADIO_INT_NONE)
    {
        power_sleep();
    }
}

/**
 * Prepare to wait for a radio interrupt type by setting the type expected
 * @param interrupt Interrupt to wait for, one of RADIO_INT_n
 */
void radio_spi_prepinterrupt(uint8_t interrupt)
{
    interrupt_state = interrupt;
}

/**
 * Handle an incoming edge on PA8, the radio interrupt pin
 */
void EXTI8_IRQHandler(void)
{
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8))
    {
        switch(interrupt_state)
        {
            case RADIO_INT_TXDONE:
                // A transmission finished, clear the flag
                interrupt_state = RADIO_INT_NONE;
                break;
            case RADIO_INT_RXREADY:
                // Payload data is waiting to be read
                power_schedule(_radio_payload_ready);
                break;
            default:
                // We're not listening for an interrupt, ignore
                break;
        }

        EXTI_ClearITPendingBit(EXTI_Line8);
    }
}
