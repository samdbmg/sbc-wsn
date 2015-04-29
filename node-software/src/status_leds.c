/**
 * Detector node status LED tree
 * Sets up storage to display status on LEDS when a switch
 * is pressed but save power otherwise
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_gpio.h"

/* Application-specific headers */
#include "status_leds.h"
#include "misc.h"
#include "power_management.h"

#define STATUS_RED_GPIO gpioPortF
#define STATUS_RED_PIN 5
#define STATUS_YELLOW_GPIO gpioPortF
#define STATUS_YELLOW_PIN 4
#define STATUS_GREEN_GPIO gpioPortF
#define STATUS_GREEN_PIN 3

#define STATUS_BUTTON_GPIO gpioPortC
#define STATUS_BUTTON_PIN 9

#define STATUS_ILLUMINATE_FLAG 0x80

// Store the LED status ready for a button push
static uint8_t status_word = 0x0;

/**
 * Set up the status storage and enable interrupts
 */
void status_init(void)
{
	// Enable the GPIO interrupt
    GPIO_PinModeSet(STATUS_BUTTON_GPIO, STATUS_BUTTON_PIN, gpioModeInputPull, 1);
    GPIO_IntConfig(STATUS_BUTTON_GPIO, STATUS_BUTTON_PIN, true, true, true);

    NVIC_EnableIRQ(GPIO_ODD_IRQn);

    // Force illumination if button is pushed (low)
    if (!GPIO_PinInGet(STATUS_BUTTON_GPIO, STATUS_BUTTON_PIN))
    {
    	status_illuminate(true);
    }
}

/**
 * Set LED status
 *
 * @param led   A combination of STATUS_x to configure
 * @param state True for on
 */
void status_led_set(uint8_t led, bool state)
{
	if (state)
	{
		status_word |= led;
	}
	else
	{
		status_word &= ~led;
	}

	if (status_word & STATUS_ILLUMINATE_FLAG)
	{
		// Force reset of LEDs if already lit
		status_illuminate(true);
	}
}

/**
 * Handle the incoming switch setting. Note that this is odd even pins, but the
 * only other GPIO interrupt is radios on PA2
 */
void GPIO_ODD_IRQHandler(void)
{
	// Check the rising edge was on the right pin
    if (GPIO_IntGet() & (0x1 << STATUS_BUTTON_PIN))
    {
    	// Find out which edge we got?
    	if (GPIO_PinInGet(STATUS_BUTTON_GPIO, STATUS_BUTTON_PIN))
    	{
    		// Switch released
    		status_illuminate(false);
    	}
    	else
    	{
    		// Switch pushed. Set some LEDs
    		status_illuminate(true);
    	}

    	GPIO_IntClear(0x1 << STATUS_BUTTON_PIN);
    }
}

/**
 * Force the status LEDs on or off
 *
 * @param active Whether to force on or force off
 */
void status_illuminate(bool active)
{
	if (active)
	{
		// Rising edge. Set some LEDs
		GPIO_PinModeSet(STATUS_RED_GPIO, STATUS_RED_PIN, gpioModePushPull, (status_word & STATUS_RED ? 1 : 0));
		GPIO_PinModeSet(STATUS_YELLOW_GPIO, STATUS_YELLOW_PIN, gpioModePushPull, (status_word & STATUS_YELLOW ? 1 : 0));
		GPIO_PinModeSet(STATUS_GREEN_GPIO, STATUS_GREEN_PIN, gpioModePushPull, (status_word & STATUS_GREEN ? 1 : 0));
		status_word |= STATUS_ILLUMINATE_FLAG;
	}
	else
	{
		GPIO_PinModeSet(STATUS_RED_GPIO, STATUS_RED_PIN, gpioModeDisabled, 0);
		GPIO_PinModeSet(STATUS_YELLOW_GPIO, STATUS_YELLOW_PIN, gpioModeDisabled, 0);
		GPIO_PinModeSet(STATUS_GREEN_GPIO, STATUS_GREEN_PIN, gpioModeDisabled, 0);
		status_word &= ~STATUS_ILLUMINATE_FLAG;
	}
}
