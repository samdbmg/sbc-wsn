/**
 * SPI interface driver for RFM69W radio module - header file
 * This code exposes functions for radio_control.c to use with the EFM32 MCU
 */
#ifndef RADIO_SPI_H_
#define RADIO_SPI_H_

// Values of interrupt state
#define RADIO_INT_NONE    0
#define RADIO_INT_RXREADY 1
#define RADIO_INT_TXDONE  2

void radio_spi_init(void);

void radio_spi_powerstate(bool state);

uint8_t radio_spi_transfer(uint8_t send_data);
void radio_spi_select(bool select);

void radio_spi_transmitwait(void);
void radio_spi_prepinterrupt(uint8_t interrupt);

#endif /* RADIO_SPI_H_ */
