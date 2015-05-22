/**
 * Defines the way the radios agree schedules, symlinked
 */

#ifndef RADIO_SCHEDULE_SETTINGS_H_
#define RADIO_SCHEDULE_SETTINGS_H_

// Period between sensor node sending beacon frames
#define RSCHED_BEACONPERIOD 15

// How long base node will remain awake each time listening for a beacon frame
#define RSCHED_WAKELENGTH 10

// How often to wake up looking for a beacon frame
#define RSCHED_WAKEPERIOD 60

// All times are in seconds

#endif /* RADIO_SCHEDULE_SETTINGS_H_ */
