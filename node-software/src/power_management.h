/**
 * Minimum power mode identification system - header file
 */

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

// Make sure this type has at least as many bits as there are items in
// power_system_t
typedef uint8_t power_system_store_t;

typedef enum {PWR_EM0, PWR_EM1, PWR_EM2, PWR_EM3} power_min_t;
typedef enum {PWR_DETECT, PWR_RADIO, PWR_SENSOR, PWR_DELAY} power_system_t;

void power_set_minimum(power_system_t system, power_min_t minimum);

void power_sleep(void);

#endif /* POWER_MANAGEMENT_H_ */
