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

static uint8_t node_addr = 0x00;

static uint8_t receive_buffer[RADIO_RECEIVE_BUFSIZE];
static uint8_t receive_read_ptr = 0;
static uint8_t receive_write_ptr = 1;

/* Functions used only in this file */
static void _radio_write_register(uint8_t address, uint8_t data);
static uint8_t _radio_read_register(uint8_t address);
static void _radio_receive_activate(bool activate);

static void _radio_read_all(void);

/**
 * Configure the radio module ready to send a receive data
 *
 * @return True if the configuration was successful
 */
bool radio_init(uint8_t addr)
{
    // Store address
    node_addr = addr;

    // Enable and configure SPI
    radio_spi_init();

    // Write a value to the sync register, then read it to check communications
    _radio_write_register(RADIO_REG_SYNCA, 0x13);

    uint8_t ret = 0x00;
    uint8_t count = 3;

    while (0x13 != ret && count > 0)
    {
        ret = _radio_read_register(RADIO_REG_SYNCA);
        count--;
    }

    // If this is true, the counter ran out before we got a response, fail
    if (0x13 != ret)
    {
        return false;
    }

    // Write all the radio config parameters
    for (uint8_t i = 0; i < (sizeof(radio_config_data) / sizeof(radio_config_data[0])); i++)
    {
        _radio_write_register(radio_config_data[i][0],
                radio_config_data[i][1]);
    }

    // Drop the radio into low power mode and kill clock supply to the USART
    radio_powerstate(false);

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
bool radio_send_data(char* data_p, uint16_t length, uint8_t dest_addr)
{
    // Make sure we got a sensible amount of data
    if (length > RADIO_MAX_PACKET_LEN)
    {
        return false;
    }

    // Trigger a receiver restart
    //_radio_write_register(RADIO_REG_PACKETCONFIG2,
    //        _radio_read_register(RADIO_REG_PACKETCONFIG2) | 0x04);

    // Kill receiver to prevent recv during send
    _radio_receive_activate(false);

    // Reconfigure interrupt pin to indicate transmission complete
    _radio_write_register(RADIO_REG_IOMAPPING, RADIO_REG_IOMAP_TXDONE);

    // Write data register address byte
    radio_spi_select(true);
    radio_spi_transfer(0x80);

    // Write length byte
    radio_spi_transfer(length + 3);

    // Write dest address
    radio_spi_transfer(dest_addr);

    // Write sender address
    radio_spi_transfer(node_addr);

    // Write payload
    for (uint8_t cursor = 0; cursor < length; cursor++)
    {
        radio_spi_transfer(data_p[cursor]);
    }

    radio_spi_select(false);

    // Prepare interrupt handler for a transmit interrupt (avoids TX race condition)
    radio_spi_transmitwait(false);

    // Flip to transmit mode to empty buffer
    _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_TX);

    // Wait for interrupt indicating buffer empty
    radio_spi_transmitwait();

    // Set the interrupt pin back to default
    _radio_write_register(RADIO_REG_IOMAPPING, RADIO_REG_IOMAP_PAYLOAD);

    // Flip back to standby mode
    _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);

    return true;
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
uint16_t radio_retrieve_data(char* data_p, uint16_t length)
{
    if (length >= RADIO_RECEIVE_BUFSIZE)
    {
        length = RADIO_RECEIVE_BUFSIZE - 1;
    }

    uint8_t next_recv_ptr = receive_read_ptr + 1;

    if (next_recv_ptr >= RADIO_RECEIVE_BUFSIZE)
    {
        next_recv_ptr = 0;
    }

    uint8_t data_count = 0;

    while (next_recv_ptr != receive_write_ptr)
    {
        *data_p = *(receive_buffer + receive_read_ptr);

        receive_read_ptr = next_recv_ptr;
        next_recv_ptr++;

        if (next_recv_ptr >= RADIO_RECEIVE_BUFSIZE)
        {
            next_recv_ptr = 0;
        }

        data_count++;

        if (data_count == length)
        {
            break;
        }
    }

    return data_count;
}

/**
 * Called by the radio_spi interrupt handler when the payload ready flag fires.
 * Reads data from the radio
 */
void _radio_payload_ready(void)
{
    radio_spi_select(true);

    // Activate read mode (this way gives sequential reads)
    radio_spi_transfer(0x00);

    uint8_t payload_size = radio_spi_transfer(0x00);

    // Make sure payload will fit in buffer
    if (payload_size > RADIO_MAX_PACKET_LEN)
    {
        payload_size = RADIO_MAX_PACKET_LEN;
    }

    // Try and grab an address
    uint8_t dest_addr = radio_spi_transfer(0x00);

    if (dest_addr == node_addr || dest_addr  == RADIO_BCAST_ADDR)
    {
        // Get data (inc sender ID)
        for (uint8_t i = 0; i <= payload_size; i++)
        {
            uint8_t data = radio_spi_transfer(0x00);

            *(receive_buffer + receive_write_ptr++) = data;

            if (receive_write_ptr >= RADIO_RECEIVE_BUFSIZE)
            {
                receive_write_ptr = 0;
            }
            else
            {
                receive_write_ptr++;
            }
        }
    }

    radio_spi_select(false);
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
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE | RADIO_REG_OPMODE_LISTENABORT);
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
void radio_powerstate(bool state)
{
    if (state)
    {
        radio_spi_powerstate(true);

        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);

        uint8_t ret = 0x00;
        while (!(ret & RADIO_REG_READYFLAG))
        {
            // Wait for radio to wake
            ret = _radio_read_register(RADIO_REG_IRQFLAGS);
        }
    }
    else
    {
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_SLEEP | RADIO_REG_OPMODE_LISTENABORT);

        while (!(_radio_read_register(RADIO_REG_IRQFLAGS) & RADIO_REG_READYFLAG))
        {
            // Wait for radio to stop listening
        }

        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_SLEEP);

        while (!(_radio_read_register(RADIO_REG_IRQFLAGS) & RADIO_REG_READYFLAG))
        {
            // Wait for radio to sleep
        }

        radio_spi_powerstate(false);
    }
}

/**
 * Read values from every register
 */
static void _radio_read_all(void)
{
    uint8_t data[80];

    uint8_t addr;

    for (addr = 0; addr < 80; addr++)
    {
        data[addr] = _radio_read_register(addr);
    }
}

