/**
 * Interface library to enable an external sensor to supply data using the
 * Low Energy UART peripheral
 *
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_leuart.h"
#include "em_gpio.h"

/* Application-specific headers */
#include "ext_sensor.h"
#include "power_management.h"
#include "detect_data_store.h"



/**
 * Configure the LEUART and wait for incoming data
 */
void ext_init(void)
{
    // Start peripheral clock (driven from LFRCO, which is started in clocks_init())
    CMU_ClockEnable(cmuClock_LEUART0, true);

    LEUART_Init_TypeDef leuartInit =
    {
            .enable = leuartEnableRx,
            .refFreq = 0,
            .baudrate = 9600,
            .databits = leuartDatabits8,
            .stopbits = leuartStopbits1,
            .parity = leuartNoParity
    };

    // Configure and enable
    LEUART_Init(LEUART0, &leuartInit);

    // Select pins used
    LEUART0->ROUTE = LEUART_ROUTE_RXPEN | LEUART_ROUTE_LOCATION_LOC0;

    // Run GPIO config for pins used
    GPIO_PinModeSet(gpioPortD, 5, gpioModeInputPull, 1); // RX
    //GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 0); // TX

    // Activate the LEUART interrupts
    NVIC_EnableIRQ(LEUART0_IRQn);
    LEUART_IntEnable(LEUART0, LEUART_IEN_RXDATAV);
}

/**
 * Handle incoming UART data by timestamping the byte into the datastore
 */
void LEUART0_IRQHandler(void)
{
    uint8_t data = LEUART_Rx(LEUART0);

    store_other(DATA_OTHER, data);

    // Interrupt flag is cleared automatically by reading buffer
}
