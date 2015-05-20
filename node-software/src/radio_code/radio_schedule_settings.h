/**
 * Defines the way the radios agree schedules, symlinked
 */

#ifndef RADIO_SCHEDULE_SETTINGS_H_
#define RADIO_SCHEDULE_SETTINGS_H_

// Period between sensor node sending beacon frames
#define RSCHED_BEACONPERIOD 10

// How long base node will remain awake each time listening for a beacon frame
#define RSCHED_WAKELENGTH 10

// How often a node should wake up to send data
#define RSCHED_NODE_PERIOD 30

// Maximum number of possible nodes
#define RSCHED_MAX_NODES 20

// Calculate time step for wakeup schedules
#define RSCHED_TIME_STEP ((RSCHED_NODE_PERIOD - RSCHED_BEACONPERIOD) / RSCHED_MAX_NODES)

// Maximum number of retries
#define RSCHED_MAX_RETRIES 3

// All times are in seconds

#endif /* RADIO_SCHEDULE_SETTINGS_H_ */