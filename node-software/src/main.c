/**
 * EFM32 Wireless Sensor Node
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_gpio.h"

/* Application-specific headers */
#include "misc.h"
#include "detect_algorithm.h"
#include "radio_control.h"
#include "power_management.h"
#include "i2c_sensors.h"

#define NODE_ADDR 0x01

uint8_t radio_test_data[] = "Hi";

void got_packet_data(uint16_t bytes)
{
    uint8_t data[5] = {0};

    // Read data from ringbuffer
    uint16_t read = radio_retrieve_data(data, 5);
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

    // Set up the error LED as an output line
    GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);

    // Configure the delay function
    misc_delay_init();

    // Initialise the radio chip
    if (!radio_init(NODE_ADDR, got_packet_data))
    {
        GPIO_PinOutSet(gpioPortC, 10);
    }

    // Configure the detection algorithm
    detect_init();

    // Configure sensors
    sensors_init();

    // Send radio test data
    radio_powerstate(true);
    radio_send_data(radio_test_data, 3, 0xFF);
    radio_receive_activate(true);

    // Read sensors
    uint16_t temp_data = sensors_read(SENS_TEMP);
    uint16_t hum_data = sensors_read(SENS_HUMID);
    uint16_t light_data = sensors_read(SENS_LIGHT);

    // Remain in sleep mode unless woken by interrupt
    while (true)
    {
        power_sleep();
    }
}

