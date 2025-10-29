// ==================================================
// File: src/GetSchedule.cpp
// ==================================================

#include "GetSchedule.h"
#include "FirebaseService.h"
#include "HeaterControl.h"
#include "Globals.h"
// #include <Firebase_ESP_Client.h>

// Global schedule data instance - no default values
ScheduleData currentSchedule = {
    NAN,   // amTemp
    NAN,   // pmTemp
    "",    // amTime
    "",    // pmTime
    -1,    // amHour
    -1,    // amMinute
    -1,    // pmHour
    -1,    // pmMinute
    false, // amEnabled
    false  // pmEnabled
};
//=======================================================
// I only want to fetch the schedule data once on startup
//                        start
//=======================================================
void fetchScheduleDataFromFirebase()
{
    Serial.println("=== Fetching Schedule Data from Firebase ===");

    bool allDataRetrieved = true;

    // Fetch AM scheduled time
    if (Firebase.RTDB.getString(&fbData, "React/schedule/amScheduledTime"))
    {
        String amTime = fbData.stringData();
        if (isValidTime(amTime))
        {
            currentSchedule.amTime = amTime;
        }

    }
    else
    {
        allDataRetrieved = false;
    }

    // Fetch PM scheduled time
    if (Firebase.RTDB.getString(&fbData, "React/schedule/pmScheduledTime"))
    {
        String pmTime = fbData.stringData();
        if (isValidTime(pmTime))
        {
            currentSchedule.pmTime = pmTime;
        }
    }
    else
    {
        allDataRetrieved = false;
    }

    // Fetch AM temperature (try both string and float formats)
    if (Firebase.RTDB.getString(&fbData, "React/schedule/amTemperature"))
    {
        String amTempStr = fbData.stringData();
        float amTemp = amTempStr.toFloat();
        if (isValidTemperature(amTemp))
        {
            currentSchedule.amTemp = amTemp;
        }
    }
    else if (Firebase.RTDB.getFloat(&fbData, "React/schedule/amTemperature"))
    {
        float amTemp = fbData.floatData();
        if (isValidTemperature(amTemp))
        {
            currentSchedule.amTemp = amTemp;
        }
    }
    else
    {
        allDataRetrieved = false;
    }

    // Fetch PM temperature (try both string and float formats)
    if (Firebase.RTDB.getString(&fbData, "React/schedule/pmTemperature"))
    {
        String pmTempStr = fbData.stringData();
        float pmTemp = pmTempStr.toFloat();
        if (isValidTemperature(pmTemp))
        {
            currentSchedule.pmTemp = pmTemp;
        }
    }
    else if (Firebase.RTDB.getFloat(&fbData, "React/schedule/pmTemperature"))
    {
        float pmTemp = fbData.floatData();
        if (isValidTemperature(pmTemp))
        {
            currentSchedule.pmTemp = pmTemp;
        }
    }
    else
    {
        allDataRetrieved = false;
    }

    if (allDataRetrieved)
    {
        // Refresh the heater control cache with the new Firebase data
        refreshScheduleCache();
    }
    getTime(); // Ensure time is fetched to parse schedule times correctly

    if (Hours >= 12)
    {
        targetTemp = currentSchedule.pmTemp;
    }
    else
    {
        targetTemp = currentSchedule.amTemp;
    }
}
//=======================================================
// I only want to fetch the schedule data once on startup
//                        end
//=======================================================

void handleScheduleUpdate(const char *topic, const String &message)
{
    // Parse topic to determine which schedule parameter to update
    String topicStr = String(topic);
    topicStr.trim();
    topicStr.toLowerCase();
    bool updateSuccessful = false;
    String firebasePath = "";

    if (topicStr.endsWith("control/schedule"))
    {
        if (topicStr == "am/hour")
        {
            String amTime = message;
            currentSchedule.amTime = amTime;
            if (isValidTime(amTime))
            {
                setAMTime(amTime);
            }
        }
        else if (topicStr == "pm/scheduledtime")
        {
            String pmTime = message;
            if (isValidTime(pmTime))
            {
                setPMTime(pmTime);
            }
        }
        else if (topicStr == "am/temperature")
        {
            float amTemp = message.toFloat();
            if (isValidTemperature(amTemp))
            {
                setAMTemperature(amTemp);
            }
        }
        else if (topicStr == "pm/temperature")
        {
            float pmTemp = message.toFloat();
            if (isValidTemperature(pmTemp))
            {
                setPMTemperature(pmTemp);
            }
        }
    }
    // Individual topic handling (AM/PM temperature, time, enabled, scheduledTime)
    if (topicStr.endsWith("/am/temperature"))
    {
        float temp = message.toFloat();
        if (isValidTemperature(temp))
        {
            setAMTemperature(temp);
            updateSuccessful = true;
            firebasePath = "/schedule/amTemperature";
        }
    }
    else if (topicStr.endsWith("/pm/temperature"))
    {
        float temp = message.toFloat();
        if (isValidTemperature(temp))
        {
            setPMTemperature(temp);
            updateSuccessful = true;
            firebasePath = "/schedule/pmTemperature";
        }
    }
    else if (topicStr.endsWith("/am/time"))
    {
        if (isValidTime(message))
        {
            setAMTime(message);
            updateSuccessful = true;
            firebasePath = "/schedule/amScheduledTime";
        }
    }
    else if (topicStr.endsWith("/pm/time"))
    {
        if (isValidTime(message))
        {
            setPMTime(message);
            updateSuccessful = true;
            firebasePath = "/schedule/pmScheduledTime";
        }
    }
    else if (topicStr.endsWith("/am/scheduledtime"))
    {
        if (isValidTime(message))
        {
            setAMTime(message);
            updateSuccessful = true;
            firebasePath = "/schedule/amScheduledTime";
        }
    }
    else if (topicStr.endsWith("/pm/scheduledtime"))
    {
        if (isValidTime(message))
        {
            setPMTime(message);
            updateSuccessful = true;
            firebasePath = "/schedule/pmScheduledTime";
        }
    }
    else if (topicStr.endsWith("/am/hour"))
    {
        int hour = message.toInt();
        currentSchedule.amHour = hour;
        updateSuccessful = true;
        firebasePath = "/schedule/amHour";
    }
    else if (topicStr.endsWith("/am/minute"))
    {
        int minute = message.toInt();
        currentSchedule.amMinute = minute;
        updateSuccessful = true;
        firebasePath = "/schedule/amMinute";
    }
    else if (topicStr.endsWith("/pm/hour"))
    {
        int hour = message.toInt();
        currentSchedule.pmHour = hour;
        updateSuccessful = true;
        firebasePath = "/schedule/pmHour";
    }
    else if (topicStr.endsWith("/pm/minute"))
    {
        int minute = message.toInt();
        currentSchedule.pmMinute = minute;
        updateSuccessful = true;
        firebasePath = "/schedule/pmMinute";
    }
    else
    {
        Serial.println(topicStr);
    }
}

bool isValidTime(const String &timeStr)
{
    // Check format HH:MM (5 characters)
    if (timeStr.length() != 5 || timeStr.charAt(2) != ':')
    {
        return false;
    }

    // Extract hours and minutes
    int hours = timeStr.substring(0, 2).toInt();
    int minutes = timeStr.substring(3, 5).toInt();

    // Validate ranges
    return (hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59);
}

bool isValidTemperature(float temp)
{
    // Allow reasonable temperature range (0-50°C)
    return (temp >= 0.0 && temp <= 50.0 && !isnan(temp));
}

// Getter functions
float getAMTemperature()
{
    return currentSchedule.amTemp;
}

float getPMTemperature()
{
    return currentSchedule.pmTemp;
}

String getAMTime()
{
    return currentSchedule.amTime;
}

String getPMTime()
{
    return currentSchedule.pmTime;
}

// Setter functions
void setAMTemperature(float temp)
{
    if (isValidTemperature(temp))
    {
        currentSchedule.amTemp = temp;
    }
}

void setPMTemperature(float temp)
{
    if (isValidTemperature(temp))
    {
        currentSchedule.pmTemp = temp;
    }
}

void setAMTime(const String &time)
{
    if (isValidTime(time))
    {
        currentSchedule.amTime = time;
    }
}

void setPMTime(const String &time)
{
    if (isValidTime(time))
    {
        currentSchedule.pmTime = time;
    }
}

float getCurrentScheduledTemperature()
{
    // Get current time to determine if we should use AM or PM temperature
    if (AmFlag)
    {
        if (!isnan(currentSchedule.amTemp))
        {
            return currentSchedule.amTemp;
        }
        else
        {
            return NAN;
        }
    }
    else
    {
        if (!isnan(currentSchedule.pmTemp))
        {
            return currentSchedule.pmTemp;
        }
        else
        {
            return NAN;
        }
    }
}

String formatTime(int hours, int minutes)
{
    String timeStr = "";
    if (hours < 10)
        timeStr += "0";
    timeStr += String(hours);
    timeStr += ":";
    if (minutes < 10)
        timeStr += "0";
    timeStr += String(minutes);
    return timeStr;
}