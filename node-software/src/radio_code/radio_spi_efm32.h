/**
 * SPI interface driver for RFM69W radio module - header file
 * This code exposes functions for radio_control.c to use with the EFM32 MCU
 */
#ifndef RADIO_SPI_EFM32_H_
#define RADIO_SPI_EFM32_H_

void radio_spi_init(void);

void radio_spi_powerstate(bool state);

uint8_t radio_spi_transfer(uint8_t send_data);
void radio_spi_select(bool select);

#endif /* RADIO_SPI_EFM32_H_ */
