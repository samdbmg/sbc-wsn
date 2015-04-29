/**
 * Activate the real time clock and use it to trigger periodic interrupts
 *
 */

/* Standard libraries */
#include <stdint.h>
#include <stdbool.h>

/* Board support headers */
#include "stm32f4xx.h"

/* Application-specific headers */
#include "rtc_driver.h"
#include "printf.h"
#include "power_management.h"
#include "base_misc.h"
#include "gsm_modem.h"

#define MODEM_WAKEUP_HOUR 3

// Function callback storage
static void (*cb_func)(void);

void RTC_Alarm_IRQHandler(void);


/**
 * Configure real time clock and oscillators
 */
void rtc_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        // Wait for oscillator to start
    }

    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);

    // Wait until all the RTC registers are synced
    RTC_WaitForSynchro();

    RTC_InitTypeDef rtcInit =
    {
            .RTC_AsynchPrediv = 100,
            .RTC_SynchPrediv = 320,
            .RTC_HourFormat = RTC_HourFormat_24
    };
    RTC_Init(&rtcInit);

    // Configure alarm B to wake at MODEM_WAKEUP_HOUR (3AM?)
    RTC_TimeTypeDef rtcBAlarmTime =
    {
            .RTC_Hours = 3,
            .RTC_Minutes = 0,
            .RTC_Seconds = 0
    };

    RTC_AlarmTypeDef rtcBAlarmInit =
    {
            .RTC_AlarmDateWeekDay = 0,
            .RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date,
            .RTC_AlarmMask = RTC_AlarmMask_None, // Alarm on all days
            .RTC_AlarmTime = rtcBAlarmTime
    };

    RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_B, &rtcBAlarmInit);
    RTC_AlarmCmd(RTC_Alarm_B, ENABLE);
    RTC_ITConfig(RTC_IT_ALRB, ENABLE);
    RTC_ClearFlag(RTC_IT_ALRB);

    NVIC_InitTypeDef nvicInit =
    {
            .NVIC_IRQChannel = RTC_Alarm_IRQn,
            .NVIC_IRQChannelPreemptionPriority = 0,
            .NVIC_IRQChannelSubPriority = 1,
            .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvicInit);
}

/**
 * Set the current RTC value to the provided time and date
 *
 * @param hour   0 - 23
 * @param minute 0 - 59
 * @param second 0 - 59
 * @param year   0 - 99
 * @param month  1 - 12
 * @param day    1 - 31
 */
void rtc_set(uint8_t hour, uint8_t minute, uint8_t second, uint8_t year,
        uint8_t month, uint8_t day)
{
    RTC_TimeTypeDef rtcTimeInit =
    {
            .RTC_Hours = hour,
            .RTC_Minutes = minute,
            .RTC_Seconds = second
    };

    RTC_DateTypeDef rtcDateInit =
    {
            .RTC_Date = day,
            .RTC_Month = month,
            .RTC_Year = year
    };

    RTC_SetTime(RTC_Format_BIN, &rtcTimeInit);
    RTC_SetDate(RTC_Format_BIN, &rtcDateInit);
}

/**
 * Return current RTC time (seconds since midnight)
 * @return Seconds since midnight
 */
uint32_t rtc_get_time_of_day(void)
{
    RTC_TimeTypeDef currentTime;
    RTC_GetTime(RTC_Format_BIN, &currentTime);

    uint32_t total_seconds = currentTime.RTC_Seconds;
    total_seconds += currentTime.RTC_Minutes * 60;
    total_seconds += currentTime.RTC_Hours * 3600;

    return total_seconds;
}

/**
 * Schedule a function to run at a specified time (overwrites past schedules)
 * @param fn   Function to call
 * @param time Timestamp at which to run function
 */
void rtc_schedule_callback(void (*fn)(void), uint32_t time)
{
    // Set up function storage
    cb_func = fn;

    // Compute wake time
    uint8_t hours = time / 3600;
    time -= hours * 3600;
    uint8_t minutes = time / 60;
    time -= minutes * 60;
    uint8_t seconds = time;

    // Configure alarm A
    RTC_TimeTypeDef rtcAlarmTime =
    {
            .RTC_Hours = hours,
            .RTC_Minutes = minutes,
            .RTC_Seconds = seconds
    };

    RTC_AlarmTypeDef rtcAlarmInit =
    {
            .RTC_AlarmDateWeekDay = 1,
            .RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date,
            .RTC_AlarmMask = RTC_AlarmMask_None, // Alarm on all days
            .RTC_AlarmTime = rtcAlarmTime
    };

    RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &rtcAlarmInit);
    RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
    RTC_ITConfig(RTC_IT_ALRA, ENABLE);
    RTC_ClearFlag(RTC_IT_ALRA);
}

/**
 * Handle alarm interrupts by dispatching functions, resetting
 */
void RTC_Alarm_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_ALRA) == SET)
    {
        // Schedule the callback
        power_schedule(cb_func);

        // Cancel the alarm
        RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
        RTC_ClearITPendingBit(RTC_IT_ALRA);
    }
    else if (RTC_GetITStatus(RTC_IT_ALRB) == SET)
    {
        // Upload data
        //TODO!

        // Clear flag
        RTC_ClearITPendingBit(RTC_IT_ALRB);
    }
}

