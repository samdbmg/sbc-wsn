/**
 * Implements access control and packet receive functions - header file
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "radio_protocol.h"
#include "radio_control.h"
#include "radio_shared_types.h"
#include "radio_schedule_settings.h"
#include "printf.h"
#include "power_management.h"
#include "base_misc.h"
#include "rtc_driver.h"

#include "tm_stm32f4_fatfs.h"

// Set this to zero to keep the radio on all the time
#define RADIO_SLEEP_IDLE 0

#define PROTO_ARRAY_SIZE 512

// Time to wait for next packet
#define PROTO_TIMEOUT_MS 750

// TIme to wait for first packet
#define PROTO_INITIAL_TIMEOUT_MS 10000

#define MAX_REPEAT 20

// Protocol state store
proto_radio_state_t proto_state = PROTO_IDLE;

// Packet handling data storage
static uint8_t incoming_data_array[PROTO_ARRAY_SIZE];
static uint16_t incoming_data_pointer = 0;
static uint8_t last_seq_number = 0;
static uint8_t seq_size;
static uint8_t seq_to_repeat[MAX_REPEAT] = {0};
static uint8_t repeat_index = 0;
static uint8_t source_node;

// Node schedule data storage
typedef struct
{
    uint8_t node_id;
    uint8_t retry_count;
    uint16_t offset;
} schedule_entry_t;

static schedule_entry_t schedule_entries[RSCHED_MAX_NODES];
static uint8_t current_schedule_point = 0;

// Functions used only in this file
void TIM2_IRQHandler(void);
static void _proto_savedata(void);
static void _proto_register_node(void);
static uint8_t _proto_add_to_schedule(uint8_t node_id);
static void _proto_endcleanup(void);

/**
 * Configure the timer used elsewhere in the protocol
 */
void proto_init(void)
{
    // Start up timer clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // Timer base configuration (TIM2, 32 bit GP timer)
    TIM_TimeBaseInitTypeDef timer_init;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_init.TIM_Prescaler = 8000;
    timer_init.TIM_Period = 1000;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timer_init);

    TIM_ARRPreloadConfig(TIM2, ENABLE);

    // Enable the overflow interrupt
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // Enable the timer's interrupts in the interrupt controller
    NVIC_InitTypeDef timer_nvic_init;
    timer_nvic_init.NVIC_IRQChannel = TIM2_IRQn;
    timer_nvic_init.NVIC_IRQChannelCmd = ENABLE;
    timer_nvic_init.NVIC_IRQChannelPreemptionPriority = 0;
    timer_nvic_init.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&timer_nvic_init);

    // Power down the timer until needed
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);

    // Mark all schedule slots as empty
    for (uint8_t i = 0; i < RSCHED_MAX_NODES; i++)
    {
        schedule_entries[i].node_id = 0xFF;
    }

    // Set up to listen for beacon frames
    proto_togglebeacon();

    // Set up the GPIO pin to power up the SD card
    GPIO_InitTypeDef gpioInit =
    {
            .GPIO_Mode = GPIO_Mode_OUT,
            .GPIO_OType = GPIO_OType_OD,
            .GPIO_Pin = GPIO_Pin_4,
            .GPIO_PuPd = GPIO_PuPd_NOPULL,
            .GPIO_Speed = GPIO_Speed_50MHz
    };
    GPIO_Init(GPIOB, &gpioInit);
    GPIO_SetBits(GPIOB, 4);
}

/**
 * Handle an incoming packet by transferring it to the packet buffer and finding
 * missing fragments
 *
 * @param bytes Number of bytes in packet
 */
void proto_incoming_packet(uint16_t bytes)
{
    // If we're waiting for beacon frames, go handle scheduling separately
    if (proto_state == PROTO_BEACON)
    {
        _proto_register_node();
    }
    else
    {
        // Reset the timeout since we got a new packet
        TIM_SetCounter(TIM2, 0);

        // Set flag that receive started
        if (proto_state == PROTO_AWAKE || proto_state == PROTO_IDLE ||
                proto_state == PROTO_RECV)
        {
            TIM_SetAutoreload(TIM2, PROTO_TIMEOUT_MS);
            TIM_Cmd(TIM2, ENABLE);
            proto_state = PROTO_RECV;
        }

        // Read in enough data to get sequence numbers
        uint8_t seq_data[3];
        uint16_t bytes_read = radio_retrieve_data(seq_data, 3);

        source_node = seq_data[0];
        seq_size = seq_data[1];
        uint8_t seq_number = seq_data[2];

        printf("\r\nGot some radio data. Count: %d of %d - %d bytes\r\n",
                seq_number, seq_size, bytes);

        // Read rest of data in at correct location
        bytes_read = radio_retrieve_data(
                (incoming_data_array + (seq_number - 1) * RADIO_MAX_DATA_LEN),
                RADIO_MAX_DATA_LEN);

        incoming_data_pointer += bytes_read;

        if (proto_state == PROTO_REPEATING)
        {
            proto_state = PROTO_ARQ;
            printf("Was a repeat\r\n");
        }
        else
        {
            while (seq_number > ++last_seq_number && repeat_index < MAX_REPEAT)
            {
                seq_to_repeat[repeat_index++] = last_seq_number;
            }

            if (seq_number == seq_size)
            {
                // Handle a complete packet by setting state
                proto_state = PROTO_ARQ;

                TIM_Cmd(TIM2, DISABLE);
            }
        }
   }

}

/**
 * Power up the radio ready for a receive
 */
void proto_start_rec(void)
{
    // Set protocol state to awake but nothing yet
    proto_state = PROTO_AWAKE;
    incoming_data_pointer = 0;
    last_seq_number = 0;
    repeat_index = 0;

    // Enable, set and start the timer
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_SetAutoreload(TIM2, PROTO_INITIAL_TIMEOUT_MS);
    TIM_Cmd(TIM2, ENABLE);

    // Radio on and listening
    radio_powerstate(true);
    radio_receive_activate(true);

    power_set_minimum(PWR_RADIO, PWR_SLEEP);
}

/**
 * The receive protocol has a state machine, drive it forward here
 */
void proto_run(void)
{
    switch (proto_state)
    {
        case PROTO_ARQ:
        {
            // We've received most of a packet, now go get the missing bits
            uint8_t packet_data[] = {PKT_REPEAT, seq_size, 0x00};

            if (repeat_index > 0)
            {
                // Delay for far end to enter receive
                misc_delay(1000, true);

                repeat_index--;

                // Set seq number to repeat
                packet_data[2] = seq_to_repeat[repeat_index];

                printf("Requesting repeat of packet %d\r\n",
                        seq_to_repeat[repeat_index]);

                // Send repeat request
                radio_send_data(packet_data, 3, source_node);

                // Reset state
                proto_state = PROTO_REPEATING;

                // Reset timer
                TIM_SetAutoreload(TIM2, PROTO_TIMEOUT_MS);
                TIM_Cmd(TIM2, ENABLE);

                // Repeat will be received by incoming_packet() and then this
                // will rerun
            }
            else
            {
                // We now have the full packet, so ACK it
                packet_data[0] = PKT_ACK;

                uint32_t time = rtc_get_time_of_day();
                packet_data[1] = (time & 0x10000) >> 16;
                packet_data[2] = time & 0xFF;
                packet_data[3] = (time & 0xFF00) >> 8;

                // Delay for far end to enter receive
                misc_delay(1000, true);

                radio_send_data(packet_data, 2, source_node);

                // Reset
                _proto_endcleanup();

                // Save the packet
                char bughit[] = "Call";
                char temp[] = "Temperature";
                char humid[] = "Humidity";
                char light[] = "Light Level";
                char other[] = "Other";

                printf("Data from node %d\r\n", source_node);

                for (uint16_t i = 0; i < incoming_data_pointer; i += 4)
                {
                    data_struct_t* data = (data_struct_t*)(incoming_data_array + i);
                    char* type;

                    switch(data->type & 0x7F)
                    {
                        case 0:
                            type = bughit;
                            break;
                        case 1:
                            type = temp;
                            break;
                        case 2:
                            type = humid;
                            break;
                        case 3:
                            type = light;
                            break;
                        default:
                            type = other;
                    }

                    uint32_t timestamp = data->time;
                    timestamp |= (data->type & 0x80) << 9;

                    if ((data->type & 0x7F) == 0)
                    {
                        // Display a different message for calls
                        printf("%d : %s - %d clicks", timestamp, type, data->otherdata & 0x7F);

                        if (data->otherdata & 0x80)
                        {
                            // Got a female
                            printf(" and female");
                        }
                        printf("\r\n");
                    }
                    else
                    {
                        printf("%d : %s - %d\r\n", timestamp, type, data->otherdata);
                    }
                }
                printf("Data done. \r\n");

                _proto_savedata();

                // Reset some stuff
                incoming_data_pointer = 0;
                last_seq_number = 0;
                repeat_index = 0;


            }

            break;
        }
        case PROTO_IDLE:
        default:
            // Nothing to do here! An interrupt will jump us forward
            break;
    }

}

/**
 * Enable or disable the waiting for beacon frame state
 */
void proto_togglebeacon()
{
    if (proto_state != PROTO_BEACON)
    {
        proto_state = PROTO_BEACON;
        radio_receive_activate(true);
        printf("Waiting for beacon frame\r\n");

        // Set to return to sleep
        rtc_schedule_callback(proto_togglebeacon, rtc_get_time_of_day() + RSCHED_WAKELENGTH);
    }
    else
    {
        radio_receive_activate(false);

#if RADIO_SLEEP_IDLE
        radio_powerstate(false);
#endif

        _proto_endcleanup();

        printf("Done waiting for beacon frame\r\n");
    }
}

/**
 * Save received data to the SD card
 */
static void _proto_savedata(void)
{
    FATFS filesystem;
    FIL data_file;

    // Power up the card
    GPIO_ResetBits(GPIOB, 4);

    // Mark that we're using GPIOD so the GSM module doesn't shut it down
    power_gpiod_use_count++;

    // Mount the disk
    if (f_mount(&filesystem, "0:", 1) != FR_OK)
    {
        printf("File system mounting failed!\r\n");

        // Kill power to some subsystems
        RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOC, DISABLE);
        RCC_APB2PeriphClockCmd (RCC_APB2Periph_SDIO, DISABLE);
        RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_DMA2, DISABLE);

        // GPIOD is used by the GSM modem, so we need to make sure its not in use
        power_gpiod_use_count--;
        if (power_gpiod_use_count == 0)
        {
            RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOC, DISABLE);
        }

        return;
    }

    // Try to open/create today's file for append
    if (f_open(&data_file, "0:testfile.csv",
            FA_OPEN_ALWAYS | FA_READ | FA_WRITE) == FR_OK)
    {
        // Opening the file succeeded, now seek to the end
        f_lseek(&data_file, f_size(&data_file));

        // Ok, now we loop through the data we got and write it
        for (uint16_t i = 0; i < incoming_data_pointer; i += 4)
        {
            // Cast the block back to a data struct
            data_struct_t* data = (data_struct_t*)(incoming_data_array + i);

            // Assemble time with the top bit
            uint32_t timestamp = data->time;
            timestamp |= (data->type & 0x80) << 9;
            data->type &= 0x7F;

            // Write out a CSV style line
            // Columns: NodeID, Time, Type, Other
            f_printf(&data_file, "%d, %d, %d, %d\n", source_node, timestamp,
                    data->type, data->otherdata);
        }

        // Close file
        f_close(&data_file);

        printf("Data written to SD card - %d lines", incoming_data_pointer/4);
    }

    // Finally unmount the card (mounting 0x0 triggers unmount)
    f_mount(0, "0:", 1);

    // Kill power to some subsystems
    RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOC, DISABLE);
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_SDIO, DISABLE);
    RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_DMA2, DISABLE);

    // GPIOD is used by the GSM modem, so we need to make sure its not in use
    power_gpiod_use_count--;
    if (power_gpiod_use_count == 0)
    {
        RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOC, DISABLE);
    }

    // Power down the card
    GPIO_SetBits(GPIOB, 4);
}

/**
 * Handle a timeout by adjusting the state machine
 */
void TIM2_IRQHandler(void)
{
    switch (proto_state)
    {
        case PROTO_AWAKE:
        {
            // We didn't get anything from this node. Assume its dead, de-register
            schedule_entries[current_schedule_point].retry_count++;

            if (schedule_entries[current_schedule_point].retry_count > RSCHED_MAX_RETRIES)
            {
                // De-register the node
                printf("Unregistering node %d\r\n", current_schedule_point);

                schedule_entries[current_schedule_point].node_id = 0xFF;
            }

            _proto_endcleanup();

            printf("Node quiet, ignoring\r\n");

            break;
        }
        case PROTO_RECV:
        {
            // Looks like the last packets were dropped! Mark for repeat
            while (++last_seq_number <= seq_size)
            {
                seq_to_repeat[repeat_index++] = last_seq_number;
            }

            proto_state = PROTO_ARQ;

            TIM_Cmd(TIM2, DISABLE);
            printf("Timer ran out, last packets lost\r\n");

            break;
        }
        case PROTO_ARQ:
        case PROTO_REPEATING:
        {
            // Well that's gone well. Call the whole thing off?
            printf("Abandoning waiting for packet\r\n");

            _proto_endcleanup();

            break;
        }
        default:
            // Do nothing
            printf("WTF did we get in state %d\r\n", proto_state);
            break;
    }

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
}

/**
 * Shut down after timeout/receive complete
 */
static void _proto_endcleanup(void)
{
    // Go back to sleep
    proto_state = PROTO_IDLE;

    TIM_Cmd(TIM2, DISABLE);

#if RADIO_SLEEP_IDLE
    // Radio off
    radio_powerstate(false);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
    power_set_minimum(PWR_RADIO, PWR_CLOCKSTOP);
#endif

    // Work out when to wake next
    uint8_t i = current_schedule_point;
    for (; i < RSCHED_MAX_NODES; i++)
    {
        if (schedule_entries[i].node_id != 0xFF)
        {
            break;
        }
    }

    // Calculate the time at the start of this period (for example, top of the hour)
    uint32_t timebase = (rtc_get_time_of_day() / RSCHED_NODE_PERIOD) * RSCHED_NODE_PERIOD;

    if (i == RSCHED_MAX_NODES)
    {
        // We've reached the beacon frame point, schedule that (waking ahead of the hour)
        rtc_schedule_callback(proto_togglebeacon, timebase +
                (RSCHED_NODE_PERIOD - RSCHED_WAKELENGTH));
        current_schedule_point = 0;

    }
    else
    {
        rtc_schedule_callback(proto_start_rec, timebase + (i * RSCHED_TIME_STEP));
        current_schedule_point = i + 1;
    }

}

/**
 * Find a free slot for a node and register it
 */
static void _proto_register_node(void)
{
    printf("Got beacon frame \r\n");

    // Read in full packet including source address
    uint8_t pkt_data[16];
    uint16_t bytes_read = radio_retrieve_data(pkt_data, 4);

    // Sanity check
    if (pkt_data[3] != PKT_BEACON)
    {
        printf("Expected a beacon frame but didn't get one, ignoring!\r\n");
        return;
    }

    // Get node address
    uint8_t node_id = pkt_data[0];

    // Find a space in the schedule for this new node
    uint8_t node_index = _proto_add_to_schedule(node_id);

    // Calculate next wakeup time
    uint32_t nextwake = (rtc_get_time_of_day() / RSCHED_NODE_PERIOD) *
            RSCHED_NODE_PERIOD +
            node_index * RSCHED_TIME_STEP;

    // ACK back to the node [time(16)], [period(16)], [nextwake(16)],[options(8)]
    uint32_t timenow = rtc_get_time_of_day();
    uint32_t period = RSCHED_NODE_PERIOD;

    pkt_data[1] = PKT_BEACONACK;

    pkt_data[1] = (timenow & 0xFF00) >> 8;
    pkt_data[2] = (timenow & 0xFF);
    pkt_data[7] = (timenow & 0x10000) >> 16;

    pkt_data[3] = (period & 0xFF00) >> 8;
    pkt_data[4] = (period & 0xFF);
    pkt_data[7] |= (period & 0x10000) >> 15;

    pkt_data[5] = (nextwake & 0xFF00) >> 8;
    pkt_data[6] = (nextwake & 0xFF);
    pkt_data[7] |= (nextwake & 0x10000) >> 14;

    radio_send_data(pkt_data, 8, node_id);
}

/**
 * Find a slot in the schedule and add a node
 * @param node_id ID of node to add
 * @return        Location of new slot
 */
static uint8_t _proto_add_to_schedule(const uint8_t node_id)
{
    uint8_t left_value = 0;
    uint8_t right_value = 0;
    uint8_t last_item_seen = 0;
    uint32_t max_diff = 0;

    for (uint8_t i = 0; i < RSCHED_MAX_NODES; i++)
    {
        // Reject unoccupied slots (0xFF is not a valid node ID)
        if (schedule_entries[i].node_id != 0xFF)
        {
            if ((i - last_item_seen) > max_diff)
            {
                // This is the new largest gap!
                left_value = last_item_seen;
                right_value = i;
                max_diff = i - last_item_seen;
            }

            last_item_seen = i;
        }
    }

    uint8_t new_index = 0;

    // Handle the special case where nothing is in the schedule
    if (left_value == 0 && right_value == 0)
    {
        // Put it directly in the middle
        new_index = RSCHED_MAX_NODES / 2;
    }
    else
    {
        // Equidistant between the two neighbour nodes
        new_index = left_value + (left_value - right_value) / 2;
    }

    // Insert new entry into the table
    schedule_entries[new_index].node_id = node_id;
    schedule_entries[new_index].retry_count = 0;

    return new_index;

}

