/**
 * Library to perform HTTP requests with Quectel M10 GSM modem - header file
 */

/* Standard libraries */
#include <base_misc.h>
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "gsm_modem.h"
#include "printf.h"
#include "base_misc.h"
#include "power_management.h"

// URLs
#define MODEM_TIME_URL "http://dark.samn.co.uk/"
#define MODEM_UPLOAD_URL "http://dark.samn.co.uk/upload/"

// Flag to indicate we got a full line of data into the buffer
bool newline_flag = false;

// Receive data buffer
#define GSM_RINGBUF_SIZE 100
static uint8_t _gsm_ringbuffer_data[GSM_RINGBUF_SIZE];

static misc_ringbuf_t gsm_ringbuffer =
{
        .data_pointer = _gsm_ringbuffer_data,
        .write_pointer = _gsm_ringbuffer_data,
        .read_pointer = _gsm_ringbuffer_data,
        .length = GSM_RINGBUF_SIZE
};

// TX buffer for benefit of sprintf
#define GSM_TXBUF_SIZE 50
static char _gsm_tx_data[GSM_TXBUF_SIZE];

// Functions only in this file
void USART2_IRQHandler(void);
static void _modem_sendstring(char *pdatastring);
static void _modem_readline(char* databuffer);
static bool _modem_waitforok(uint8_t lines);
static void _modem_waitforlines(uint8_t lines);

/**
 * Enable and configure the serial port to use the modem
 */
void modem_setup(void)
{
    // Keep us awake
    power_set_minimum(PWR_MODEM, PWR_SLEEP);

    // Turn GPIOD on and USART2 (and mark we're using GPIO)
    power_gpiod_use_count++;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // Set the IO pins to AF mode
    GPIO_InitTypeDef gpioAFInit =
    {
            .GPIO_Mode = GPIO_Mode_AF,
            .GPIO_OType = GPIO_OType_PP,
            .GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6,
            .GPIO_PuPd = GPIO_PuPd_NOPULL,
            .GPIO_Speed = GPIO_Speed_25MHz
    };
    GPIO_Init(GPIOD, &gpioAFInit);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2); // TX
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2); // RX

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
    USART_Init(USART2, &usartInit);

    // Configure the USART incoming data interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // Enable USART2 interrupts
    NVIC_InitTypeDef nvicInit =
    {
            .NVIC_IRQChannel = USART2_IRQn,
            .NVIC_IRQChannelPreemptionPriority = 0,
            .NVIC_IRQChannelSubPriority = 1,
            .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvicInit);

    // Configure the reset pin
    GPIO_InitTypeDef gpioInit =
    {
            .GPIO_Mode = GPIO_Mode_OUT,
            .GPIO_OType = GPIO_OType_PP,
            .GPIO_Pin = GPIO_Pin_7,
            .GPIO_PuPd = GPIO_PuPd_NOPULL,
            .GPIO_Speed = GPIO_Speed_25MHz
    };
    GPIO_Init(GPIOD, &gpioInit);

    uint8_t config_attempts = 5;
    while (config_attempts-- > 1)
    {
        // Reset the modem
        GPIO_WriteBit(GPIOD, GPIO_Pin_7, SET);
        misc_delay(2000, true);
        GPIO_WriteBit(GPIOD, GPIO_Pin_7, RESET);
        misc_delay(300, true);

        // If receive is on before reset the power down message is seen
        misc_ringbuffer_clear(&gsm_ringbuffer);
        USART_Cmd(USART2, ENABLE);

        // Send the wake AT command
        sprintf(_gsm_tx_data, "AT\r");
        _modem_sendstring(_gsm_tx_data);

        // Wait for a response and check it was "OK"
        misc_delay(500, true);
        char returndata[100];
        _modem_readline(returndata);
        _modem_readline(returndata);

        if (returndata[0] == 'O' && returndata[1] == 'K')
        {
            break;
        }
        else
        {
            USART_Cmd(USART2, DISABLE);

            // Modem wasn't shut down properly last time, now we wait for it to
            // reset
            misc_delay(10000, true);
        }
    }

    if (config_attempts == 0)
    {
        printf("Modem init failed badly\r\n");

        while (1)
        {

        }
    }

    // Now we sit and wait for Call Ready
    _modem_waitforlines(3);

    misc_delay(10000, true);

    misc_ringbuffer_clear(&gsm_ringbuffer);
}

/**
 * Power down the modem
 */
void modem_shutdown(void)
{
    // Reset the modem
    GPIO_WriteBit(GPIOD, GPIO_Pin_7, SET);
    misc_delay(2000, true);
    GPIO_WriteBit(GPIOD, GPIO_Pin_7, RESET);

    // Turn off the USART
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);

    // Turn of GPIOD if we can
    power_gpiod_use_count--;
    if (power_gpiod_use_count == 0)
    {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, DISABLE);
    }

    // Allow clockstop
    power_set_minimum(PWR_MODEM, PWR_CLOCKSTOP);
}

/**
 * Enable or disable the GPRS connection
 * @param enable New state of connection
 */
void modem_connection(bool enable)
{
    if (enable)
    {

        misc_delay(5000, true);
        // Enable context 0
        sprintf(_gsm_tx_data, "AT+QIFGCNT=0\r");
        _modem_sendstring(_gsm_tx_data);
        _modem_waitforok(2);

        // Connect to GPRS
        sprintf(_gsm_tx_data, "AT+QICSGP=1,\"everywhere\"\r");
        _modem_sendstring(_gsm_tx_data);
        _modem_waitforok(2);

        // Register TCP stack
        sprintf(_gsm_tx_data, "AT+QIREGAPP\r");
        _modem_sendstring(_gsm_tx_data);
        _modem_waitforok(2);

        // Activate
        sprintf(_gsm_tx_data, "AT+QIACT\r");
        _modem_sendstring(_gsm_tx_data);
        _modem_waitforok(2);

        misc_delay(5000, true);
    }
    else
    {
        // Deactivate
        sprintf(_gsm_tx_data, "AT+QIDEACT\r");
        _modem_sendstring(_gsm_tx_data);
        _modem_waitforok(2);
    }
}

/**
 * Retrieve the time from the network
 * @return Timestamp
 */
uint32_t modem_gettime(void)
{
    // Submit URL to connect to
    sprintf(_gsm_tx_data, "AT+QHTTPURL=%d,30\r", sizeof(MODEM_TIME_URL) - 1);
    _modem_sendstring(_gsm_tx_data);

    _modem_waitforlines(2);
    misc_delay(5000, true);
    sprintf(_gsm_tx_data, "%s\r", MODEM_TIME_URL);
    _modem_sendstring(_gsm_tx_data);
    _modem_waitforok(1);

    misc_delay(5000, true);

    // Run GET request
    sprintf(_gsm_tx_data, "AT+QHTTPGET=60\r\n");
    _modem_sendstring(_gsm_tx_data);
    _modem_waitforok(2);

    misc_delay(5000, true);

    // Retrieve response
    sprintf(_gsm_tx_data, "AT+QHTTPREAD=60\r");
    _modem_sendstring(_gsm_tx_data);

    misc_delay(5000, true);

    modem_shutdown();

    char responsedata[1024];
    misc_ringbuffer_read(&gsm_ringbuffer, responsedata, 1024);

    // Parse the data
    //TODO

    return 0;
}

/**
 * POST a block of data (a file) to the server
 *
 * @param data_p  Pointer to start of block
 * @param length  Length of data block
 * @param address Web address to write to
 */
void modem_sendfile(uint8_t* data_p, uint16_t length, char address[])
{

}

/**
 * Handle a data received interrupt by copying data into the buffer
 */
void USART2_IRQHandler(void)
{
    // Confirm interrupt was data received
    if (USART2->SR & (0x1 << 5))
    {
        char incoming = (char)USART_ReceiveData(USART2);

        // Insert new data into receive buffer
        misc_ringbuffer_write(&gsm_ringbuffer, &incoming, 1);

        // Check if latest byte was a newline
        if (incoming == '\n')
        {
            newline_flag = true;
        }
    }
}

/**
 * Send a null-terminated string of data to the GSM modem
 *
 * @param pdatastring Pointer to a string of data to send
 */
static void _modem_sendstring(char *pdatastring)
{
    while (*pdatastring)
    {
        USART_SendData(USART2, (uint16_t)*pdatastring++);

        while (!(USART2->SR & USART_SR_TXE))
        {
            // Wait for TX buffer to empty
        }

        while (!(USART2->SR & USART_SR_TC))
        {
            // Wait for TX to end
        }
    }
}

/**
 * Read a line out of the ringbuffer
 * @param databuffer Pointer to a string to put the line in
 */
static void _modem_readline(char* databuffer)
{
    do
    {
        misc_ringbuffer_read(&gsm_ringbuffer, databuffer, 1);

        if (*databuffer == 0x0)
        {
            break;
        }
    }
    while (*databuffer++ != '\n');

    // Null terminate
    *databuffer = 0x0;
}

/**
 * Sit and wait until the modem responds "OK"
 *
 * @param lines Number of lines expected (including the one OK will be on)
 * @return True on success, False on error
 */
static bool _modem_waitforok(uint8_t lines)
{
    char newdata[100];

    _modem_waitforlines(lines);

    // Command is repeated in first line
    _modem_readline(newdata);
    _modem_readline(newdata);

    if (newdata[0] == 'O' && newdata[1] == 'K' && newdata[2] == '\r')
    {
    }
    else
    {
        printf("Got modem error: %s", newdata);
        return false;
    }

    misc_ringbuffer_clear(&gsm_ringbuffer);
    return true;
}

/**
 * Wait for the modem to send a specified number of lines
 * @param lines Number of lines to wait on
 */
static void _modem_waitforlines(uint8_t lines)
{
    while (lines-- > 0)
    {
        newline_flag = false;
        while (!newline_flag)
        {
            power_sleep();
        }
    }
}


