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
// float targetTemp = 0.0;

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

// Heater control function
void updateHeaterControl(SystemStatus &status)
{
    // getTime();
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
    pushTargetTempToFirebase((float)(round(targetTemp * 10) / 10.0)); // // Display current values BEFORE control logic
    const float HYSTERESIS = 0.25;                                    // degrees - Balanced for responsive control while preventing oscillation
    //***************************************
    // Keep this code No. 1
    //***************************************
    if (tempRed > targetTemp + HYSTERESIS)
    // if (tempRed > targetTemp)
    {

        digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
        status.heater = HEATERS_OFF;   // Update to use new enum
        publishSystemData();
        updateLEDs(status);

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
    //************************************
    // No2
    else if (tempRed < targetTemp)
    {
        digitalWrite(RELAY_PIN, LOW);                // Changed: LOW = Relay ON
        double currentReading = getCurrentReading(); // Get current voltage reading for heater state analysis
        HeaterState heaterState = getHeaterState(currentReading);
        status.heater = heaterState;
        ;
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

        unsigned long now = millis(); // Declare now here so it's available for all conditions
        if (heaterState == ONE_HEATER_ON)
        {
#if DEBUG_SERIAL
            Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
            Serial.print("Time since last email: ");
            Serial.print((now - last_Single_Heater_Alert_Email) / 60000);
            Serial.println(" minutes");
#endif
            if (first_Run_Single_Heater_Alert)
            {

#if DEBUG_SERIAL
                Serial.println("Sending FIRST single heater failure email");
#endif
                first_Run_Single_Heater_Alert = false;
                last_Single_Heater_Alert_Email = now;
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure Attention needed", message);
            }

            // Check if 30 minutes have passed since last email attempt
            else if (now - last_Single_Heater_Alert_Email >= 60000UL) // 1800000UL
            {
#if DEBUG_SERIAL
                Serial.println("30 minutes elapsed - sending repeat single heater failure email");
#endif
                last_Single_Heater_Alert_Email = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure Attention needed", message);
            }
        }
        if (heaterState == BOTH_HEATERS_BLOWN)
        {
#if DEBUG_SERIAL
            Serial.println("🚨🚨🚨 Both heaters detected as BLOWN 🚨🚨🚨");
            Serial.print("Current reading: ");
            Serial.print(currentReading, 2);
            Serial.println(" A");
            Serial.print("Time since last email: ");
            Serial.print((now - last_Total_Failure_Email) / 60000);
            Serial.println(" minutes");
#endif

            if (first_Run_Total_Failure)
            {
#if DEBUG_SERIAL
                Serial.println("Sending FIRST total failure email");
#endif

                first_Run_Total_Failure = false;
                last_Total_Failure_Email = now;
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("Immediate attention required! CRITICAL: Total Heater Failure", message);
            }

            else if (now - last_Total_Failure_Email >= 3600000UL)
            {
#if DEBUG_SERIAL
                Serial.println("1 hour elapsed - sending repeat total failure email");
#endif

                last_Total_Failure_Email = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("Immediate attention required! CRITICAL: Total Heater Failure", message);
            }
        }
        if (heaterState == BOTH_HEATERS_ON)
            // Reset failure flags when both heaters confirmed working
            if (!first_Run_Single_Heater_Alert)
            {
#if DEBUG_SERIAL
                Serial.println("✅ Both heaters working properly - resetting single heater failure flag");
#endif
                first_Run_Single_Heater_Alert = true;
            }
    }
    else
    {
        // Temperature is within hysteresis band - maintain current state
        publishSystemData();
        updateLEDs(status);

#if DEBUG_SERIAL
        Serial.println("🌡️ Temperature within hysteresis band - maintaining current state");
#endif
    } // End of main temperature control if-else structure
} // End of updateHeaterControl function

// // Function to refresh the cached schedule values
// // Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
}
