/**
 * EFM32 Node power demo main file
 */

#include "em_device.h"
#include "em_prs.h"
#include "em_timer.h"
#include "em_adc.h"
#include "em_dma.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_rtc.h"
#include "em_usart.h"
#include "em_gpio.h"

#include "adc_mode.h"
#include "pin_mode.h"

//#define ADC_MODE
#define PINCHANGE_MODE

#define WAKEUP_INTERVAL_MS 10000

// Calculate RTC wakeup timeout
#define LFRCO_FREQ 32768
#define RTC_WAKEUP_TICKS (((LFRCO_FREQ * WAKEUP_INTERVAL_MS) / 1000) - 1)


/**
 * Configure the RTC and start it, with interrupt enabled
 */
void rtc_init(void)
{
    // Start the low frequency oscillator LFRCO
    CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);

    // Enable the RTC clock supply
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_RTC, true);

    RTC_Init_TypeDef rtc_init_data;
    rtc_init_data.comp0Top = true;
    rtc_init_data.debugRun = false;
    rtc_init_data.enable = true;

    RTC_CompareSet(0, RTC_WAKEUP_TICKS);

    // Enable RTC interrupt and start it
    RTC_IntEnable(RTC_IF_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);
    RTC_Init(&rtc_init_data);

}

/**
 * Handle an RTC wakeup by doing something with data
 */
void RTC_IRQHandler(void)
{
    CMU_ClockEnable(cmuClock_USART1, true);

    rtc_handler();

    CMU_ClockEnable(cmuClock_USART1, false);

    // Clear pending interrupt
    RTC_IntClear(RTC_IFC_COMP0);
}

void spi_init(void)
{
    CMU_ClockEnable(cmuClock_USART1, true);

    // Pin config
    GPIO_PinModeSet(gpioPortD, 6, gpioModeInput, 0);     // MISO
    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 0);  // MOSI
    GPIO_PinModeSet(gpioPortC, 14, gpioModePushPull, 0); // CS
    GPIO_PinModeSet(gpioPortC, 15, gpioModePushPull, 0); // CLK

    USART_InitSync_TypeDef spi_init_data;

    spi_init_data.autoTx = false;
    spi_init_data.clockMode = usartClockMode0;
    spi_init_data.databits = usartDatabits8;
    spi_init_data.baudrate = 1000000;
    spi_init_data.enable = true;
    spi_init_data.master = true;
    spi_init_data.msbf = false;
    spi_init_data.prsRxEnable = false;
    spi_init_data.refFreq = 0;

    // Turn SPI on
    USART_InitSync(USART1, &spi_init_data);

    USART1->ROUTE = USART_ROUTE_TXPEN | USART_ROUTE_CLKPEN | USART_ROUTE_RXPEN |
            USART_ROUTE_CSPEN | USART_ROUTE_LOCATION_LOC3;

    USART1->CTRL |= USART_CTRL_AUTOCS;

    USART1->CMD |= USART_CMD_TXEN;

    CMU_ClockEnable(cmuClock_USART1, false);
}

/**
 * Main function. Initialises peripherals and lets interrupts do the work.
 */
int main(void)
{
    CHIP_Init();

    CMU_HFRCOBandSet(cmuHFRCOBand_21MHz);

    // Low energy module clock supply
    CMU_ClockEnable(cmuClock_CORELE, true);

    // GPIO pins clock supply
    CMU_ClockEnable(cmuClock_GPIO, true);

    // Start the RTC, we'll worry about time later
    rtc_init();

    // Configure USART for PSI
    spi_init();

#ifdef ADC_MODE
    // Start up the ADC, timer and DMA
    adc_mode_init();
#elif defined(PINCHANGE_MODE)
    pin_mode_init();
#endif

    while (1)
    {
#ifdef ADC_MODE
        EMU_EnterEM1();
#elif defined(PINCHANGE_MODE)
        sleep_handler();
#endif
    }
}

