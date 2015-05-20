/**
 * Environment sensors interface - header file
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Peripheral control headers */
#include "em_device.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_i2c.h"
#include "printf.h"

/* Application-specific headers */
#include "i2c_sensors.h"
#include "power_management.h"
#include "misc.h"

#define SENS_TEMP_ADDR 0x40
#define SENS_LIGHT_ADDR 0x29

static bool _sensors_send(uint8_t addr, uint16_t flags, uint8_t txlen,
        uint8_t rxlen);

// Create some storage for I2C transfers
static uint8_t _sens_tx_buffer[3];
static uint8_t _sens_rx_buffer[3];

static I2C_TransferSeq_TypeDef _sens_transfer =
{
        .buf =
        {
            {
                    .data = _sens_tx_buffer,
                    .len = sizeof(_sens_tx_buffer)
            },
            {
                    .data = _sens_rx_buffer,
                    .len = sizeof(_sens_rx_buffer)
            }
        }
};

// Store state of I2C transfer
static volatile bool _sens_i2c_active = false;

/**
 * Configure the sensors and interface
 *
 * @return True on success, false on timeout
 */
bool sensors_init(void)
{
    CMU_ClockEnable(cmuClock_I2C0, true);
    power_set_minimum(PWR_SENSOR, PWR_EM2);

    I2C_Init_TypeDef const i2cInit =
    {
            .clhr = i2cClockHLRStandard,
            .enable = true,
            .freq = 10000,
            .master = true,
            .refFreq = 0
    };

    // Pin configuration
    GPIO_PinModeSet(gpioPortE, 13, gpioModeWiredAndPullUpFilter, 0);
    GPIO_PinModeSet(gpioPortE, 12, gpioModeWiredAndPullUpFilter, 0);

    // Enable pin location
    I2C0->ROUTE = I2C_ROUTE_SDAPEN | I2C_ROUTE_SCLPEN | I2C_ROUTE_LOCATION_LOC6;

    // Enable interrupts
    NVIC_ClearPendingIRQ(I2C0_IRQn);
    NVIC_EnableIRQ(I2C0_IRQn);

    // Enable I2C
    I2C_Init(I2C0, &i2cInit);

    I2C0->CTRL |= I2C_CTRL_AUTOACK | I2C_CTRL_AUTOSN;

    // Configure the light-level sensor to have 10 integration cycles
    _sens_tx_buffer[0] = 0xA0;
    _sens_tx_buffer[1] = 0x01;
    _sens_tx_buffer[2] = 0xF6;
    if (!(_sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE, 3, 0)))
	{
    	return false;
	}

    // And 4x gain
    _sens_tx_buffer[0] = 0x8F;
    _sens_tx_buffer[1] = 0x01;
    if (!(_sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE, 2, 0)))
	{
    	return false;
	}

    // And put it back to sleep
    _sens_tx_buffer[0] = 0x80;
    _sens_tx_buffer[1] = 0x00;
    if (!(_sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE, 2, 0)))
	{
    	return false;
	}

    // Power down until needed
    CMU_ClockEnable(cmuClock_I2C0, false);
    power_set_minimum(PWR_SENSOR, PWR_EM3);

    return true;

}

uint16_t sensors_read(sensor_type_t type)
{
    power_set_minimum(PWR_SENSOR, PWR_EM2);
    CMU_ClockEnable(cmuClock_I2C0, true);

    uint16_t result;
    uint16_t raw_data;

    switch (type)
    {
        case SENS_TEMP:
            // Prepare some instructions
            _sens_tx_buffer[0] = 0xE3;

            // Request temperature
            _sensors_send(SENS_TEMP_ADDR, I2C_FLAG_WRITE_READ, 1, 3);

            // Convert back to a real value
            raw_data = _sens_rx_buffer[1] & 0xFC;
            raw_data |= (((uint16_t)_sens_rx_buffer[0]) << 8);

            // Using formula from datasheet section 6.2, scaled to 100x for
            // avoidance of floating point operations
            uint64_t temp_result = -4685 + (17572 * ((uint64_t)raw_data * 100)) / 6553600;
            result = (uint16_t)(temp_result/100);
            break;

        case SENS_HUMID:
            // Prepare some instructions
            _sens_tx_buffer[0] = 0xE5;

            // Request humidity
            _sensors_send(SENS_TEMP_ADDR, I2C_FLAG_WRITE_READ, 1, 3);

            // Convert back to a real value
            raw_data = _sens_rx_buffer[1] & 0xFC;
            raw_data |= (((uint16_t)_sens_rx_buffer[0]) << 8);

            // Using formula from datasheet section 6.1
            uint32_t humid_result = -6 + (125 * (uint32_t)raw_data) / 65536;
            result = (uint16_t)(humid_result);
            break;

        case SENS_LIGHT:
            // First wake the sensor and start a conversion
            _sens_tx_buffer[0] = 0x80;
            _sens_tx_buffer[1] = 0x03;
            _sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE, 2, 0);

            // Prepare to read the status register
            _sens_tx_buffer[0] = 0x93;

            // Wait for conversion completion or timeout
            uint8_t attempts = 0;
            while (attempts++ < 3)
            {
                misc_delay(30, true);

                _sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE_READ, 1, 3);

                if (_sens_rx_buffer[0] & 0x01)
                {
                    attempts = 3;
                }
            }

            // Go and read the data for "clear"
            _sens_tx_buffer[0] = 0xB4;
            _sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE_READ, 1, 3);

            raw_data = _sens_rx_buffer[0];
            raw_data |= ((uint16_t)_sens_rx_buffer[1]) << 8;

            // Now power down the sensor
            _sens_tx_buffer[0] = 0x80;
            _sens_tx_buffer[1] = 0x00;
            _sensors_send(SENS_LIGHT_ADDR, I2C_FLAG_WRITE, 2, 0);

            result = raw_data;
            break;

        default:
            break;
    }

    CMU_ClockEnable(cmuClock_I2C0, false);
    power_set_minimum(PWR_SENSOR, PWR_EM3);

    return result;
}

/**
 * Fill in and send an instruction to an I2C sensor, wait for it to complete
 * (sleeps when waiting). Don't set rxlen or txlen to be longer than their
 * buffer sizes
 *
 * @param addr  Address of slave I2C sensor. Will be left shifted by 1 for R/W flag
 * @param flags I2C_FLAG_x flags
 * @param txlen Number of bytes from TX buffer to write
 * @param rxlen Number of bytes to read into RX buffer
 * @return		False if command timed out
 */
static bool _sensors_send(uint8_t addr, uint16_t flags, uint8_t txlen,
        uint8_t rxlen)
{
    // Configure transfer parameters
    _sens_transfer.addr = (addr << 1);
    _sens_transfer.flags = flags;
    _sens_transfer.buf[0].len = txlen;
    _sens_transfer.buf[1].len = rxlen;

    // Set transfer flag
    _sens_i2c_active = true;

    // Actually start sending commands
    I2C_TransferInit(I2C0, &_sens_transfer);

    // Delay before attempting wait
    misc_delay(1, true);

    // Set a non-blocking delay as a timeout
    misc_delay(250, false);

    // Wait for transfer completion
    while (_sens_i2c_active)
    {
        power_sleep();

        if (!misc_delay_active())
        {
        	// Timer ran out, exit with failure
        	return false;
        }
    }

    return true;
}

/**
 * Handle the I2C interrupt by triggering another state machine step
 */
void I2C0_IRQHandler(void)
{
    if (_sens_i2c_active)
    {
        I2C_TransferReturn_TypeDef ret = I2C_Transfer(I2C0);

        if (ret != i2cTransferInProgress)
        {
            _sens_i2c_active = false;
        }
    }
}
