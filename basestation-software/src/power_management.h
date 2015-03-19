/**
 * Minimum power mode identification system and power and clock management -
 * header file
 */

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

// Make sure this type has at least as many bits as there are items in
// power_system_t
typedef uint8_t power_system_store_t;

typedef enum {PWR_WAKE, PWR_SLEEP, PWR_CLOCKSTOP} power_min_t;
typedef enum {PWR_RADIO, PWR_DELAY} power_system_t;

void power_set_minimum(power_system_t system, power_min_t minimum);

void power_sleep(void);

void power_schedule(void (*fn)(void));

#endif /* POWER_MANAGEMENT_H_ */
