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
static bool firstRunSingleFailure = true;
static bool firstRunTotalFailure = true;
static unsigned long lastSingleFailureEmail = 0;
static unsigned long lastTotalFailureEmail = 0;
static HeaterState lastKnownState = BOTH_HEATERS_ON; // Assume both working initially

// Heater control function
void updateHeaterControl(SystemStatus &status)
{

    // Serial.println("******************Updating Heater Control...**************");
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
    float tempRed = getTemperature(0); // Before the if statement

    float newTargetTemp = AmFlag ? currentSchedule.amTemp : currentSchedule.pmTemp;

    String scheduledTime = AmFlag ? currentSchedule.amTime : currentSchedule.pmTime;
    Serial.println("😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈");
    // Serial.println("Current Time: " + currentTime);
    // Serial.println("Scheduled Time: " + scheduledTime);
    // Serial.println("================================================");
    // Serial.print("New Target Temp: ");
    // Serial.println(newTargetTemp);
    // Serial.print("Current Target Temp: ");
    // Serial.println(targetTemp);
    // Serial.print("Current Time: ");
    // Serial.println(currentTime);
    // Serial.print("Scheduled Time: ");
    // Serial.println(scheduledTime);
    // Serial.println("😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈");
    // Only update targetTemp at the scheduled time
    if (scheduledTime == currentTime)
    {
        targetTemp = newTargetTemp;
        // Serial.println("👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺");
        // Serial.println("==================================================");
        // Serial.println("Debug output for target temperature update");
        // Serial.print("newTargetTemp: ");
        // Serial.println(newTargetTemp);
        // Serial.print("Target temperature updated to: ");
        // Serial.println(targetTemp);
        // Serial.println("==================================================");
    }
    publishSingleValue("esp32/control/targetTemperature", (float)(round(targetTemp * 10) / 10.0));
    pushTargetTempToFirebase((float)(round(targetTemp * 10) / 10.0)); // // Display current values BEFORE control logic
    // Serial.print("************* Target Temperature **************: ");
    // Serial.println(targetTemp);
    // Serial.print("pmTime ");
    // Serial.println(currentSchedule.pmTime);
    // Serial.print("amTime ");
    // Serial.println(currentSchedule.amTime);
    // Serial.print("pmTemp ");
    // Serial.println(currentSchedule.pmTemp);
    // Serial.print("amTemp ");
    // Serial.println(currentSchedule.amTemp);
    // Serial.print("Current Red Sensor: ");
    // Serial.print(tempRed);
    // Serial.println("°C");
    // Serial.println("*******************************");
    const float HYSTERESIS = 1; // degrees
                                // Check if the current target temperature is valid
    Serial.println("❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎❎");
    // Serial.print("Current Target Temperature: ");
    // Serial.println(targetTemp);
    // Serial.print("Current Red Sensor Temperature: ");
    // Serial.println(tempRed);
    // Serial.println("*******************************");
    Serial.println("💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥");
    Serial.println("tempRed: ");
    Serial.println(tempRed);
    Serial.println("targetTemp: ");
    Serial.println(targetTemp);

    Serial.println("💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥");
    Serial.println("Heater Control Decision Making...");
    Serial.println("💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥💥");
    if (tempRed > targetTemp + HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
        status.heater = HEATERS_OFF;   // Update to use new enum
        publishSystemData();
        updateLEDs(status);
        // Serial.println("🔥 Heater OFF - tempRed > targetTemp + HYSTERESIS");
    }

    else if (tempRed < targetTemp - HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, LOW); // Changed: LOW = Relay ON
        Serial.println("🔥 Heater ON - tempRed < targetTemp - HYSTERESIS");
        // Use current sensor to determine actual heater state

        // status.heater = getHeaterState(currentReading);
        status.heater = BOTH_HEATERS_ON;   // Update to use new enum
        double currentReading = getCurrentReading(); // Get current reading for heater state analysis

        HeaterState heaterState = getHeaterState(currentReading);
        Serial.println("currentReading (A): ");
        Serial.println(currentReading);
        publishSystemData();
        updateLEDs(status);

        // Enhanced heater failure detection with multi-heater support
        unsigned long now = millis(); // Declare now here so it's available for all conditions

        Serial.println("🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥 ");
        Serial.println("Heater State : ");
        Serial.println(heaterState);
        Serial.println("🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥 ");
        

        if (heaterState == ONE_HEATER_ON)
        {
            status.heater = ONE_HEATER_ON;
            updateLEDs(status);
            Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️");
            Serial.println("Only one heater is drawing current!");
            Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️");
        }

        if (heaterState == BOTH_HEATERS_BLOWN)
        {
            status.heater = BOTH_HEATERS_BLOWN;
            updateLEDs(status);
            Serial.println("🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨");
            // Total heater failure - both heaters not working
            if (firstRunTotalFailure)
            {
                firstRunTotalFailure = false;
                lastTotalFailureEmail = now;
                sendEmail("CRITICAL: Total Heater Failure",
                          "Both heaters have failed! Current: " + String(currentReading, 2) + "A. Immediate attention required!");
                Serial.println("🚨 CRITICAL: Total heater failure detected!");
            }

            // Reset flag after 30 minutes
            if (!firstRunTotalFailure && (now - lastTotalFailureEmail >= 1800000UL))
            {
                firstRunTotalFailure = true;
            }
        }
        else if (heaterState == ONE_HEATER_ON && lastKnownState == BOTH_HEATERS_ON)
        {
            // Single heater failure - one heater stopped working
            if (firstRunSingleFailure)
            {
                firstRunSingleFailure = false;
                lastSingleFailureEmail = now;
                sendEmail("WARNING: Single Heater Failure",
                          "One heater has failed. Current: " + String(currentReading, 2) + "A (expected ~4.6A). System still operational but reduced efficiency.");
                Serial.println("⚠️ WARNING: Single heater failure detected!");
            }

            // Reset flag after 30 minutes
            if (!firstRunSingleFailure && (now - lastSingleFailureEmail >= 1800000UL))
            {
                firstRunSingleFailure = true;
            }
        }

        // Update last known state for comparison
        if (heaterState != BOTH_HEATERS_BLOWN)
        {
            lastKnownState = heaterState;
        }

        // Debug output
        Serial.print("🔥 Heater Status: ");
        switch (heaterState)
        {
        case HEATERS_OFF:
            Serial.println("NO HEATERS NORMAL");
            break;
        case ONE_HEATER_ON:
            Serial.println("ONE HEATER");
            break;
        case BOTH_HEATERS_ON:
            Serial.println("BOTH HEATERS");
            break;
        case BOTH_HEATERS_BLOWN:
            Serial.println("BOTH HEATERS BLOWN");
            break;
        }
    }
    else
    {
        // Heaters working properly - reset failure flags when current is detected
        firstRunSingleFailure = true;
        firstRunTotalFailure = true;
        Serial.println("✅ Heaters operating normally");
    }
    // else
    // {
    //     Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️ ");
    //     Serial.println("⚠️  Warning: Heater should be ON but no current detected! Possible issue with heater or wiring.");
    //     Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️");
    //     status.heater = HEATER_ERROR;
    publishSystemData();
    updateLEDs(status);
    // }

} // else, keep current state

// // Function to refresh the cached schedule values
// // Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
    Serial.println("🔄 Schedule cache refresh requested - will update on next heater control cycle");
}
