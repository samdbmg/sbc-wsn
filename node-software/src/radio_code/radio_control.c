/**
 * Protocol control for RFM69W radio module - header file
 * This code should provide high-level control of the module without making any
 * processor-specific calls so should be fairly portable
 *
 * Be careful, this file is shared between base and node software!
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Application-specific headers */
#include "radio_control.h"
#include "radio_spi.h"
#include "radio_config.h"

static uint8_t node_addr = 0x00;

// Receiver ring buffer
static uint8_t receive_buffer[RADIO_RECEIVE_BUFSIZE];
static uint16_t receive_read_ptr = 0;
static uint16_t receive_write_ptr = 0;

// Callback to run when receiving a new packet
void (*_radio_packet_callback)(uint16_t) = 0x0;

// Flag to indicate current radio state
static radio_state_t _radio_state = RADIO_SLEEP;

/* Functions used only in this file */
static void _radio_write_register(uint8_t address, uint8_t data);
static uint8_t _radio_read_register(uint8_t address);

static void _radio_read_all(void);

/**
 * Configure the radio module ready to send a receive data
 *
 * @param  addr     Address of this node
 * @param  callback Function to call when getting a packet ready in the buffer
 * @return          True if the configuration was successful
 */
bool radio_init(uint8_t addr, void (*callback)(uint16_t))
{
    // Store address and callback
    node_addr = addr;
    _radio_packet_callback = callback;

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
bool radio_send_data(uint8_t* data_p, uint16_t length, uint8_t dest_addr)
{
    _radio_read_all();

    // Make sure we got a sensible amount of data
    if (length > RADIO_MAX_PACKET_LEN)
    {
        return false;
    }


    // Restart RX to avoid deadlock
    _radio_write_register(RADIO_REG_PACKETCONFIG2, 0x16);

    // Find out if we're receiving to reset when done
    bool recv_active = (_radio_state == RADIO_LISTEN);

    // Kill receiver to prevent recv during send
    radio_receive_activate(false);

    // Reconfigure interrupt pin to indicate transmission complete
    _radio_write_register(RADIO_REG_IOMAPPING, RADIO_REG_IOMAP_TXDONE);

    // Write data register address byte
    radio_spi_select(true);
    radio_spi_transfer(0x80);

    // Write length byte
    radio_spi_transfer((uint8_t)(length + 2));

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
    radio_spi_prepinterrupt(RADIO_INT_TXDONE);

    // Flip to transmit mode to empty buffer
    _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_TX);

    // Wait for interrupt indicating buffer empty
    radio_spi_transmitwait();

    // Set the interrupt pin back to default
    _radio_write_register(RADIO_REG_IOMAPPING, RADIO_REG_IOMAP_PAYLOAD);

    // Flip back to standby or receive mode
    radio_receive_activate(recv_active);

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
uint16_t radio_retrieve_data(uint8_t* data_p, uint16_t length)
{
    if (length >= RADIO_RECEIVE_BUFSIZE)
    {
        length = RADIO_RECEIVE_BUFSIZE - 1;
    }

    uint8_t data_count = 0;

    while (receive_read_ptr != receive_write_ptr)
    {
        *data_p++ = *(receive_buffer + receive_read_ptr++);

        if (receive_read_ptr >= RADIO_RECEIVE_BUFSIZE)
        {
            receive_read_ptr = 0;
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
    // Disable receive
    radio_receive_activate(false);


    radio_spi_select(true);

    // Address validation success flag
    bool packet_accepted = false;

    // Activate read mode (this way gives sequential reads)
    radio_spi_transfer(0x00);

    uint8_t payload_size = radio_spi_transfer(0x00);

    // Make sure payload will fit in buffer
    uint16_t space_left;
    if (receive_read_ptr > receive_write_ptr)
    {
        space_left = (uint16_t)(receive_read_ptr - receive_write_ptr - 1);
    }
    else
    {
        space_left = (uint16_t)(RADIO_RECEIVE_BUFSIZE - receive_write_ptr + receive_read_ptr - 1);
    }

    // Try and grab an address
    uint8_t dest_addr = radio_spi_transfer(0x00);

    if ((dest_addr == node_addr || dest_addr == RADIO_BCAST_ADDR) &&
            (payload_size - 1 <= space_left))
    {
        packet_accepted = true;

        // Get data (inc sender ID)
        // Remove one byte from payload size to omit destination
        for (uint8_t i = 0; i < payload_size - 1; i++)
        {
            uint8_t data = radio_spi_transfer(0x00);

            *(receive_buffer + receive_write_ptr++) = data;

            if (receive_write_ptr >= RADIO_RECEIVE_BUFSIZE)
            {
                receive_write_ptr = 0;
            }

        }
    }
    else
    {
        packet_accepted = false;
    }

    radio_spi_select(false);

    // Turn receive back on
    radio_receive_activate(true);

    // Run callback function if packet valid
    if (packet_accepted)
    {
        _radio_packet_callback((uint16_t)(payload_size - 1));
    }
}

/**
 * Turn receive mode on or off
 *
 * @param activate Set to true to switch receive on, false to turn off
 */
void radio_receive_activate(bool activate)
{
    if (activate)
    {
        // Restart RX to avoid deadlock
        _radio_write_register(RADIO_REG_PACKETCONFIG2, 0x16);

        radio_spi_prepinterrupt(RADIO_INT_RXREADY);
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_RX);
        _radio_state = RADIO_LISTEN;
    }
    else
    {
        radio_spi_prepinterrupt(RADIO_INT_NONE);
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE | RADIO_REG_OPMODE_LISTENABORT);
        _radio_write_register(RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE);
        _radio_state = RADIO_WAKE;
    }

    // Wait for mode ready
    while (!(_radio_read_register(RADIO_REG_IRQFLAGS) & RADIO_REG_READYFLAG))
    {

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
    address &= (uint8_t)(~0x80);

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

        _radio_state = RADIO_WAKE;
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

        _radio_state = RADIO_SLEEP;
    }
}


/**
 * Read values from every register (function does nothing if debug mode is off)
 */
static void _radio_read_all(void)
{
#ifdef DEBUG_RADIO
    uint8_t data[80];

    uint8_t addr;

    for (addr = 0; addr < 80; addr++)
    {
        data[addr] = _radio_read_register(addr);
    }
#endif
}
