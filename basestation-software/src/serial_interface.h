/**
 * Configure a USART peripheral to act as a serial debug interface - header file
 *
 */

#ifndef SERIAL_INTERFACE_H_
#define SERIAL_INTERFACE_H_

void serial_init(void);
void serial_print(char* string);


#endif /* SERIAL_INTERFACE_H_ */
