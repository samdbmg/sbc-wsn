/**
 * Configuration variables for RFM69W radio module, plus register definitions
 */

#ifndef RADIO_CONFIG_H_
#define RADIO_CONFIG_H_

// Register addresses used directly (rather than just as part of config)
#define RADIO_REG_SYNCA 0x2F
#define RADIO_REG_OPMODE 0x01
#define RADIO_REG_PACKETCONFIG2 0x3D
#define RADIO_REG_IRQFLAGS 0x27
#define RADIO_REG_IOMAPPING 0x25

// Register values and masks
#define RADIO_REG_OPMODE_WAKE 0x04
#define RADIO_REG_OPMODE_SLEEP 0x00
#define RADIO_REG_OPMODE_TX 0x0C
#define RADIO_REG_OPMODE_RX 0x44
#define RADIO_REG_OPMODE_LISTENABORT 0x20

#define RADIO_REG_IOMAP_TXDONE 0x00
#define RADIO_REG_IOMAP_PAYLOAD 0x40

#define RADIO_REG_READYFLAG 0x80

/* Configuration array (based on method used by Felix Ruso in Moteino code at
   https://github.com/LowPowerLab/RFM69/ */
uint8_t radio_config_data[][2] =
{
        {RADIO_REG_OPMODE, RADIO_REG_OPMODE_WAKE}, // RegOpMode - Sequencer on, Listen on, Standby mode
        {0x02, 0x00}, // RegDataModul - Packet mode, FSK
        {0x03, 0x1a}, // RegBitrateMSB - 4.8kbps
        {0x04, 0x0b}, // RegBitrateLSB - 4.8kbps
        {0x05, 0x00}, // RegFdevMSB - 5khz
        {0x06, 0x52}, // RegFdevLSB - 5khz
        {0x07, 0xd9}, // RegFrfMSB - 868MHz
        {0x08, 0x00}, // RegFrfMID - 868MHz
        {0x09, 0x00}, // RegFrfLSB - 868MHz
        {0x0A, 0x9F}, // RegPaLevel - PA0 on, Power = 13dBm
        {0x19, 0x42}, // RegRxBW - DDC freq default,  Mant 16, Exp 2
        {RADIO_REG_IOMAPPING, RADIO_REG_IOMAP_PAYLOAD}, // RegDioMapping - DIO0 Payload Ready
        {0x26, 0x07}, // RegDioMapping2 - Clock out off
        {0x28, 0x10}, // RegIrqFlags2 - Clear FIFO and flags
        {0x29, 220 }, // RegRssiThresh - Threshold for RSSI trigger, -110dBm
        {0x2E, 0x98}, // RegSyncConfig - Sync on, FIFO on SyncAddress, 2 byte sync word no errors
        {0x2F, 0x2D}, // RegSyncValue1 - Set first part of sync word
        {0x30, 0x64}, // RegSyncValue2 - Set second part of sync word
        {0x37, 0x10}, // RegPacketConfig1 - Variable length, DC free off, CRC on, clear off, address off
        {0x38, 66  }, // RegPayloadLength - Max payload 66 bytes
        {0x3C, 0x8F}, // RegFifoThresh - Transmit when bits available, threshold is 15
        {0x3D, 0x12}, // RegPacketConfig2 - 2 bit restart delay, Auto restart on
        {0x6F, 0x30}, // RegTestDagc - improve fading margin
};

#endif /* RADIO_CONFIG_H_ */
