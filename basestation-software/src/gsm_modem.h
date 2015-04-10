/**
 * Library to perform HTTP requests with Quectel M10 GSM modem - header file
 */

#ifndef GSM_MODEM_H_
#define GSM_MODEM_H_

void modem_setup(void);
void modem_shutdown(void);
void modem_connection(bool enable);

uint32_t modem_gettime(void);
void modem_sendfile(uint8_t* data_p, uint16_t length, char address[]);


#endif /* GSM_MODEM_H_ */
