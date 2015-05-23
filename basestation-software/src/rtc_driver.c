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
    // Power up the RTC and enable access
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    // Reset it
    RCC_BackupResetCmd(ENABLE);
    RCC_BackupResetCmd(DISABLE);

    // Start the clock
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

    RTC_BypassShadowCmd(ENABLE);

    // Set the RTC for now
    rtc_set(0, 0, 0, 15, 1, 1);
    // Enable alarm EXTI
    EXTI_ClearITPendingBit(EXTI_Line17);
    EXTI_InitTypeDef extiInit =
    {
            .EXTI_Line = EXTI_Line17,
            .EXTI_Mode = EXTI_Mode_Interrupt,
            .EXTI_Trigger = EXTI_Trigger_Rising,
            .EXTI_LineCmd = ENABLE
    };
    EXTI_Init(&extiInit);

    // Configure alarm B to wake at MODEM_WAKEUP_HOUR (3AM?)
    NVIC_InitTypeDef nvicInit =
    {
            .NVIC_IRQChannel = RTC_Alarm_IRQn,
            .NVIC_IRQChannelPreemptionPriority = 0,
            .NVIC_IRQChannelSubPriority = 0,
            .NVIC_IRQChannelCmd = ENABLE
    };
    NVIC_Init(&nvicInit);

    RTC_AlarmTypeDef rtcBAlarmInit =
    {
            .RTC_AlarmDateWeekDay = RTC_Weekday_Monday,
            .RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date,
            .RTC_AlarmMask = RTC_AlarmMask_DateWeekDay, // Alarm on all days
            .RTC_AlarmTime =
            {
                    .RTC_Hours = 3,
                    .RTC_Minutes = 0,
                    .RTC_Seconds = 10
            }
    };
    //TODO Do we really want bin here?
    RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_B, &rtcBAlarmInit);

    RTC_AlarmCmd(RTC_Alarm_B, ENABLE);
    RTC_ITConfig(RTC_IT_ALRB, ENABLE);
    RTC_ClearITPendingBit(RTC_IT_ALRA);
    RTC_ClearITPendingBit(RTC_IT_ALRB);


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
            .RTC_H12 = RTC_H12_AM,
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

    RTC_WaitForSynchro();

    // Wait for RTC to update
    bool time_sync_complete = false;
    RTC_TimeTypeDef time_match;
    while (!time_sync_complete)
    {
        RTC_GetTime(RTC_Format_BIN, &time_match);

        if (time_match.RTC_Hours == rtcTimeInit.RTC_Hours &&
                time_match.RTC_Minutes == rtcTimeInit.RTC_Minutes &&
                time_match.RTC_Seconds == rtcTimeInit.RTC_Seconds)
        {
            time_sync_complete = true;
        }
    }
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
    total_seconds += currentTime.RTC_Minutes * 60u;
    total_seconds += currentTime.RTC_Hours * 3600u;

    return total_seconds;
}

/**
 * Return a string containing the date
 * @param date Pointer to a string for date in YY-MM-DD form
 */
void rtc_get_date_string(char* date)
{
    RTC_DateTypeDef currentDate;
    RTC_GetDate(RTC_Format_BIN, &currentDate);

    sprintf(date, "%02d-%02d-%02d", currentDate.RTC_Year, currentDate.RTC_Month,
            currentDate.RTC_Date);
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
    uint8_t hours = (uint8_t)(time / 3600u);
    time -= hours * 3600u;
    uint8_t minutes = (uint8_t)(time / 60u);
    time -= minutes * 60u;
    uint8_t seconds = (uint8_t)time;

    // Configure alarm A
    RTC_AlarmTypeDef rtcAlarmInit =
    {
            .RTC_AlarmDateWeekDay = 1,
            .RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date,
            .RTC_AlarmMask = RTC_AlarmMask_DateWeekDay, // Alarm on all days
            .RTC_AlarmTime =
            {
                    .RTC_Hours = hours,
                    .RTC_Minutes = minutes,
                    .RTC_Seconds = seconds
            }
    };

    RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &rtcAlarmInit);
    RTC_ClearITPendingBit(RTC_IT_ALRA);
    RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
    RTC_ITConfig(RTC_IT_ALRA, ENABLE);
}

/**
 * Handle alarm interrupts by dispatching functions, resetting
 */
void RTC_Alarm_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_ALRA) == SET)
    {
        // Cancel the alarm
        RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
        RTC_ClearITPendingBit(RTC_IT_ALRA);

        // Schedule the callback
        power_schedule(cb_func);
    }
    else if (RTC_GetITStatus(RTC_IT_ALRB) == SET)
    {
        // Upload data
        //TODO!

        // Clear flag
        RTC_ClearITPendingBit(RTC_IT_ALRB);
    }

    EXTI_ClearITPendingBit(EXTI_Line17);
}

