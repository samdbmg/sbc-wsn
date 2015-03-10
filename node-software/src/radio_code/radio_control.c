/**
 * Protocol control for RFM69W radio module - header file
 * This code should provide high-level control of the module without making any
 * processor-specific calls so should be fairly portable
 */

// Standard libraries
#include <stdint.h>
#include <stdbool.h>

/* Application-specific includes */
#include "radio_control.h"
#include "radio_spi_efm32.h"
#include "radio_config.h"

/* Functions used only in this file */
static void _radio_write_register(uint8_t address, uint8_t data);
static uint8_t _radio_read_register(uint8_t address);
static void _radio_powerstate(bool state);

/**
 * Configure the radio module ready to send a receive data
 *
 * @return True if the configuration was successful
 */
bool radio_init(void)
{
    // Enable and configure SPI
    radio_spi_init();

    // Write a value to the sync register, then read it to check communications
    _radio_write_register(RADIO_REG_SYNCA, 0xAA);

    if (0xAA != _radio_read_register(RADIO_REG_SYNCA))
    {
        return false;
    }

    // Write all the radio config parameters
    for (uint8_t i = 0; i < sizeof(radio_config_data); i++)
    {
        _radio_write_register(radio_config_data[i][0],
                radio_config_data[i][1]);
    }

    // Drop the radio into low power mode and kill clock supply to the USART
    _radio_powerstate(false);

    return true;
}

/**
 * Send a prepared packet over the link. Blocks until TX complete
 *
 * @param data_p  Pointer to the data to be sent
 * @param length  Number of bytes to send. No bounds checking on array is done
 * @param dest_addr Destination address to send to. 0x00 for broadcast
 * @return        True on send success
 */
bool _radio_send_data(char* data_p, uint16_t length, uint8_t dest_addr)
{
    // Trigger a receiver restart
    _radio_write_register(RADIO_REG_PACKETCONFIG2,
            _radio_read_register(RADIO_REG_PACKETCONFIG2) | 0x04);

    // Kill receiver to prevent recv during send
    _radio_activate_receiver(false);

    // Wait for mode update to complete
    while (!_radio_read_register(RADIO_REG_IRQFLAGS & 0x80))
    {

    }

    // Reconfigure interrupt pin to indicate transmission complete
    _radio_write_register(RADIO_REG_IOMAPPING, 0x00);

    // Start sending data
    uint16_t data_cursor = 0;

    while (length > 0)
    {
        uint8_t packet_len;
        if (length > RADIO_MAX_PACKET_LEN)
        {
            length -= RADIO_MAX_PACKET_LEN;
            packet_len = RADIO_MAX_PACKET_LEN;
        }
        else
        {
            packet_len = length;
            length = 0;
        }

        // Write data register address byte
        radio_spi_select(true);
        radio_spi_transfer(0x00);

        // Write length byte
        radio_spi_transfer(packet_len + 1);

        // Write dest address
        radio_spi_transfer(dest_addr);

        // Write payload
        for (; packet_len > 0; packet_len--)
        {
            radio_spi_transfer(data_p[data_cursor++]);
        }

        radio_spi_select(false);

        // Flip to transmit mode to empty buffer
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_TX);

        // Wait for interrupt indicating buffer empty
        radio_spi_transmitwait();

        // Flip back to standby mode
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);
    }

    return false;
}

/**
 *
 * Retrieve the contents of the receive buffer and clear the buffer itself
 *
 * @param data_p  Pointer to store the data from the buffer
 * @param length  Number of bytes to retrieve from the buffer
 * @return        Number of bytes retrieved. If 0, buffer empty. If equal to
 *                length parameter, call again as there may be more
 */
uint16_t _radio_retrieve_data(char* data_p, uint16_t length)
{
    return false;
}

/**
 * Called by the radio_spi interrupt handler when the payload ready flag fires.
 * Reads data from the radio
 */
void _radio_payload_ready(void)
{
    //TODO
}

/**
 * Turn receive mode on or off
 *
 * @param activate Set to true to switch receive on, false to turn off
 */
void _radio_receive_activate(bool activate)
{
    if (activate)
    {
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_RX);
    }
    else
    {
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);
    }
}

/**
 * Write data to a single register in the radio
 *
 * @param address Register address to write to
 * @param data    Data byte to be written
 */
static void _radio_write_register(uint8_t address, uint8_t data)
{
    // Ensure address has write flag set
    address |= 0x80;

    // Write the address first, then the data
    radio_spi_select(true);
    radio_spi_transfer(address);
    radio_spi_transfer(data);
    radio_spi_select(false);
}

/**
 * Read data from a single register in the radio
 *
 * @param address Register address to read
 * @return        Data received from radio after read
 */
static uint8_t _radio_read_register(uint8_t address)
{
    // Ensure address has write flag cleared
    address &= ~0x80;

    // Write the address first, then read data
    radio_spi_select(true);
    radio_spi_transfer(address);
    uint8_t data = radio_spi_transfer(0);
    radio_spi_select(false);

    return data;
}

/**
 * Enable or disable the radio for power reduction. Also shuts down SPI
 * @param state True to power up the radio, false to shutdown
 */
static void _radio_powerstate(bool state)
{
    if (state)
    {
        radio_spi_powerstate(true);

        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);
    }
    else
    {
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_SLEEP);

        radio_spi_powerstate(false);
    }
}

