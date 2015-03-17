/**
 * SPI interface driver for RFM69W radio module
 * This code exposes functions for radio_control.c to use with the EFM32 MCU
 */

// Standard libraries
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_usart.h"
#include "em_gpio.h"
#include "em_emu.h"

/* Application-specific headers */
#include "radio_spi.h"
#include "misc.h"
#include "radio_control.h"
#include "power_management.h"

// Type of interrupt currently being waited on
static volatile uint8_t interrupt_state = RADIO_INT_NONE;

/**
 * Configure the SPI peripheral and pins to talk to the radio
 */
void radio_spi_init(void)
{
    // Enable module clock and power up
    CMU_ClockEnable(cmuClock_USART1, true);

    // Base configuration of USART to SPI mode. Generated by EnergyAware
    // Designer tool
    USART_InitSync_TypeDef usart_init = USART_INITSYNC_DEFAULT;

    usart_init.baudrate     = 1000000;
    usart_init.databits     = usartDatabits8;
    usart_init.msbf         = 1;
    usart_init.master       = 1;
    usart_init.clockMode    = usartClockMode0;
    usart_init.prsRxEnable  = 0;
    usart_init.autoTx       = 0;

    USART_InitSync(USART1, &usart_init);

    // Configure the USART to use location 3, enable all the pins
    USART1->ROUTE = USART_ROUTE_TXPEN | USART_ROUTE_CLKPEN | USART_ROUTE_RXPEN |
            USART_ROUTE_LOCATION_LOC3;

    // Disable automatic control of the CS line
    USART1->CTRL &= ~USART_CTRL_AUTOCS;

    // Enable transmit and receive
    USART1->CMD |= USART_CMD_TXEN | USART_CMD_RXEN;

    // Pin setup
    GPIO_PinModeSet(gpioPortD, 6, gpioModeInput, 0);     // MISO
    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 0);  // MOSI
    GPIO_PinModeSet(gpioPortC, 14, gpioModePushPull, 0); // CS
    GPIO_PinModeSet(gpioPortC, 15, gpioModePushPull, 0); // CLK

    // Set NSS high
    radio_spi_select(false);

    // Configure the pin change interrupt for DIO0
    GPIO_PinModeSet(gpioPortB, 11, gpioModeInput, 0);
    GPIO_IntConfig(gpioPortB, 11, true, false, true);

    NVIC_EnableIRQ(GPIO_ODD_IRQn);

}

/**
 * Enable or disable clock to the SPI module
 * @param state True to power up, false to power down
 */
void radio_spi_powerstate(bool state)
{
    CMU_ClockEnable(cmuClock_USART1, state);
}

/**
 * Send and receive single bytes of data
 *
 * @param send_data A byte of data to send, set to zero for a read
 * @return          Received byte
 */
uint8_t radio_spi_transfer(uint8_t send_data)
{
    return USART_SpiTransfer(USART1, send_data);
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
        GPIO_PinOutClear(gpioPortC, 14);
    }
    else
    {
        GPIO_PinOutSet(gpioPortC, 14);
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
 * Handle an incoming edge on PB11, the radio interrupt pin
 */
void GPIO_ODD_IRQHandler(void)
{
    // Check the rising edge was on the right pin
    if ((GPIO_IntGet() & (0x1 << 11)) && GPIO_PinInGet(gpioPortB, 11))
    {
        switch(interrupt_state)
        {
            case RADIO_INT_TXDONE:
                // A transmission finished, clear the flag
                interrupt_state = RADIO_INT_NONE;
                break;
            case RADIO_INT_RXREADY:
                // Payload data is waiting to be read
                _radio_payload_ready();
                break;
            default:
                // We're not listening for an interrupt, ignore
                break;
        }

        GPIO_IntClear(0x1 << 11);
    }
}