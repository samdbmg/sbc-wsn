/**
 * Environment sensors interface - header file
 */

#ifndef I2C_SENSORS_H_
#define I2C_SENSORS_H_

typedef enum {SENS_TEMP, SENS_HUMID, SENS_LIGHT} sensor_type_t;

bool sensors_init(void);

uint16_t sensors_read(sensor_type_t sensor);

#endif /* I2C_SENSORS_H_ */
