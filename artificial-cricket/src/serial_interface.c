/**
 * Configure a USART peripheral to act as a serial debug interface - header file
 *
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "serial_interface.h"

void putchar(char c);

/**
 * Configure USART1 to use as a debug output
 */
void serial_init(void)
{
    // Turn GPIOB on and USART1
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // Set the IO pins to AF mode
    GPIO_InitTypeDef gpioInit =
    {
            .GPIO_Mode = GPIO_Mode_AF,
            .GPIO_OType = GPIO_OType_PP,
            .GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7,
            .GPIO_PuPd = GPIO_PuPd_NOPULL,
            .GPIO_Speed = GPIO_Speed_25MHz
    };
    GPIO_Init(GPIOB, &gpioInit);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1); // TX
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1); // RX

    // Configure the USART
    USART_InitTypeDef usartInit =
    {
            .USART_BaudRate = 9600,
            .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
            .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
            .USART_Parity = USART_Parity_No,
            .USART_StopBits = USART_StopBits_1,
            .USART_WordLength = USART_WordLength_8b
    };
    USART_Init(USART1, &usartInit);

    USART_Cmd(USART1, ENABLE);

}

/**
 * Print a string on the serial port
 * @param string A null terminated string
 */
void serial_print_string(char* string)
{
    while(*string)
    {
        serial_print_char(*string++);
    }
}

/**
 * Print a single character - used by the printf library
 *
 * @param c      Character to print
 */
void serial_print_char(char c)
{
    USART_SendData(USART1, (uint16_t)c);

    while (!(USART1->SR & USART_SR_TXE))
    {
        // Wait for TX buffer to empty
    }

    while (!(USART1->SR & USART_SR_TC))
    {
        // Wait for TX to end
    }
}

void putchar(char c)
{
    serial_print_char(c);
}
