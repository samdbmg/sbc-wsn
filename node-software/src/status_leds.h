/**
 * Detector node status LED tree - header file
 */

#ifndef STATUS_LEDS_H_
#define STATUS_LEDS_H_

#define STATUS_RED 0x4
#define STATUS_YELLOW 0x2
#define STATUS_GREEN 0x1

void status_init(void);

void status_led_set(uint8_t led, bool state);

void status_illuminate(bool active);


#endif /* STATUS_LEDS_H_ */
