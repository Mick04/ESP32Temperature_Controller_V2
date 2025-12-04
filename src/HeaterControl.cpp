//==============================
// Target Temperature Management
//==============================
float targetTemp = 0; // persistent across calls
float getTargetTemp()
{
    return targetTemp;
}

//==============================
// Heater Control
//==============================
#include <Arduino.h>
#include "HeaterControl.h"
#include "TemperatureSensors.h"
#include "GetSchedule.h"
#include "TemperatureSensors.h"
#include "StatusLEDs.h"
#include "TimeManager.h"
#include "MQTTManager.h"
#include "FirebaseService.h"
#include "Config.h"
#include "Globals.h"
#include "Send_E-Mail.h"

// External declarations
bool AmFlag = false;
bool SINGLE_HEATER_OFF = false;

// Global flag to force schedule cache refresh
static bool forceScheduleRefresh = false;

// Multi-heater failure detection state variables
static bool first_Run_Single_Failure = true;
static bool first_Run_Single_Heater_Alert = true; // Separate flag for 1.91A case
static bool first_Run_Total_Failure = true;
static unsigned long last_Single_Failure_Email = 0;
static unsigned long last_Single_Heater_Alert_Email = 0; // Separate timer for 1.91A case
static unsigned long last_Total_Failure_Email = 0;
static HeaterState lastKnownState = BOTH_HEATERS_ON; // Assume both working initially

// Forward declaration
void check_E_Mail_Timers(double currentReading, unsigned long currentMillis);

// Heater control function
void updateHeaterControl(SystemStatus &status)
{
    getTime();                               // Updates Hours and Minutes
    String currentTime = getFormattedTime(); // Returns "HH:MM"
    
    if (currentTime < "12:00")
    {
        AmFlag = true;
    }
    else
    {
        AmFlag = false;
    }
    
    readAllSensors();
    float tempRed = getTemperature(0); // Get the value of the Red sensor

    float newTargetTemp = AmFlag ? currentSchedule.amTemp : currentSchedule.pmTemp;
    String scheduledTime = AmFlag ? currentSchedule.amTime : currentSchedule.pmTime;

    if (scheduledTime == currentTime)
    {
        targetTemp = newTargetTemp;
#if DEBUG_SERIAL
        Serial.println("==================================================");
        Serial.println("Debug output for target temperature update");
        Serial.print("newTargetTemp: ");
        Serial.println(newTargetTemp);
        Serial.print("Target temperature updated to: ");
        Serial.println(targetTemp);
        Serial.println("==================================================");
#endif
    }
    
    publishSingleValue("esp32/control/targetTemperature", (float)(round(targetTemp * 10) / 10.0));
    pushTargetTempToFirebase((float)(round(targetTemp * 10) / 10.0));
    
    const float HYSTERESIS = 0.25; // degrees - Balanced for responsive control while preventing oscillation
    
    //***************************************
    // Temperature above target + hysteresis - Turn OFF
    //***************************************
    if (tempRed > targetTemp + HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, HIGH); // HIGH = Relay OFF
        status.heater = HEATERS_OFF;
        publishSystemData();
        updateLEDs(status);
        
        // Check email timers even when heater is off (if single heater is failed)
        if (SINGLE_HEATER_OFF)
        {
            unsigned long currentMillis = millis();
            check_E_Mail_Timers(0.0, currentMillis); // Pass 0.0 as we don't have current reading when off
        }

#if DEBUG_SERIAL
        Serial.println("❄️❄️❄️ Heater OFF ❄️❄️❄️");
        Serial.print("Current temp: ");
        Serial.print(tempRed);
        Serial.print("°C, Target: ");
        Serial.print(targetTemp);
        Serial.print("°C, Hysteresis: ");
        Serial.println(HYSTERESIS);
        Serial.println("❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️");
#endif
    }
    //***************************************
    // Temperature below target - Turn ON and monitor heater state
    //***************************************
    else if (tempRed < targetTemp)
    {
        digitalWrite(RELAY_PIN, LOW); // LOW = Relay ON
        double currentReading = getCurrentReading(); // Get current voltage reading for heater state analysis
        HeaterState heaterState = getHeaterState(currentReading);
        status.heater = heaterState;
        updateLEDs(status);
        
#if DEBUG_SERIAL
        Serial.println("♨️♨️♨️ Heater ON ♨️♨️♨️");
        Serial.print("Current reading: ");
        Serial.print(currentReading, 2);
        Serial.println(" A");
        Serial.print("Heater state: ");
        Serial.println(heaterStateToString(heaterState));
        Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
#endif

        publishSystemData();
        unsigned long currentMillis = millis();
        
        // Handle ONE_HEATER_ON state
        if (heaterState == ONE_HEATER_ON)
        {
#if DEBUG_SERIAL
            Serial.println("⚠️⚠️⚠️ One heater detected as ON ⚠️⚠️⚠️");
            Serial.print("Time since last email: ");
            Serial.print((currentMillis - last_Single_Heater_Alert_Email));
            Serial.println(" minutes");
#endif
            if (first_Run_Single_Heater_Alert)
            {
#if DEBUG_SERIAL
                Serial.println("Sending FIRST single heater failure email");
#endif
                first_Run_Single_Heater_Alert = false;
                last_Single_Heater_Alert_Email = currentMillis;
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~1.6-2.5A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure - Attention Needed", message);
                SINGLE_HEATER_OFF = true;
            }
            else
            {
                // Check for reminder emails
                check_E_Mail_Timers(currentReading, currentMillis);
            }
        }
        // Handle BOTH_HEATERS_BLOWN state
        else if (heaterState == BOTH_HEATERS_BLOWN)
        {
#if DEBUG_SERIAL
            Serial.println("🚨🚨🚨 Both heaters detected as BLOWN 🚨🚨🚨");
            Serial.print("Current reading: ");
            Serial.print(currentReading, 2);
            Serial.println(" A");
            Serial.print("Time since last email: ");
            Serial.print((currentMillis - last_Total_Failure_Email) / 60000);
            Serial.println(" minutes");
            Serial.print(" first_Run_Total_Failure: ");
            Serial.println(first_Run_Total_Failure);
#endif

            if (first_Run_Total_Failure)
            {
#if DEBUG_SERIAL
                Serial.println("Sending FIRST total failure email");
#endif
                first_Run_Total_Failure = false;
                last_Total_Failure_Email = currentMillis;
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("WARNING: Both Heater Failure - Attention Needed", message);
            }
            else if (currentMillis - last_Total_Failure_Email >= 1800000UL) // 30 minutes
            {
#if DEBUG_SERIAL
                Serial.println("30 minutes elapsed - sending repeat total failure email");
#endif
                last_Total_Failure_Email = currentMillis;
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("WARNING: Both Heater Failure - Attention Needed", message);
            }
        }
        // Handle BOTH_HEATERS_ON state - Reset all failure flags
        else if (heaterState == BOTH_HEATERS_ON)
        {
            if (!first_Run_Single_Heater_Alert || !first_Run_Total_Failure || SINGLE_HEATER_OFF)
            {
#if DEBUG_SERIAL
                Serial.println("✅ Both heaters working properly - resetting all failure flags");
#endif
                first_Run_Single_Heater_Alert = true;
                first_Run_Total_Failure = true;
                SINGLE_HEATER_OFF = false;
            }
        }
    }
    //***************************************
    // Temperature within hysteresis band - maintain current state
    //***************************************
    else
    {
        publishSystemData();
        updateLEDs(status);

#if DEBUG_SERIAL
        Serial.println("🌡️ Temperature within hysteresis band - maintaining current state");
#endif
    }
} // End of updateHeaterControl function

// Function to refresh the cached schedule values
// Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
}

// Function to check and send reminder emails for single heater failure
void check_E_Mail_Timers(double currentReading, unsigned long currentMillis)
{
    // Check if 60 minutes have passed since last email attempt
    if (currentMillis - last_Single_Heater_Alert_Email >= 3600000UL) // 3600000 ms = 60 minutes
    {
#if DEBUG_SERIAL
        Serial.println("10 minutes elapsed - sending repeat single heater failure email");
#endif
        last_Single_Heater_Alert_Email = currentMillis;
        char message[200];
        sprintf(message, "One heater has failed. Current: %.2fA (expected ~1.6-2.5A). System still operational but reduced efficiency.", currentReading);
        sendEmail("WARNING: Single Heater Failure - Attention Needed", message);
    }
}