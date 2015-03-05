/**
 * Configuration variables for RFM69W radio module, plus register definitions
 */

#ifndef RADIO_CONFIG_H_
#define RADIO_CONFIG_H_

// Register addresses used directly (rather than just as part of config)
#define RADIO_REG_SYNCA 0x2F
#define RADIO_REG_OPMODE 0x01

// Register values and masks
#define RADIO_REG_OPMODE_WAKE 0x24
#define RADIO_REG_OPMODE_SLEEP 0x20

/* Configuration array (based on method used by Felix Ruso in Moteino code at
   https://github.com/LowPowerLab/RFM69/ */
uint8_t radio_config_data[][2] =
{
        {0x01, RADIO_REG_OPMODE_WAKE}, // RegOpMode - Sequencer on, Listen on, Standby mode
        {0x02, 0x01}, // RegDataModul - Packet mode, FSK, GFSK BT=1.0
};

#endif /* RADIO_CONFIG_H_ */
