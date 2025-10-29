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

    if (scheduledTime == currentTime)
    {
        targetTemp = newTargetTemp;
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
    const float HYSTERESIS = .0; // degrees - Increased from 0.1 to compensate for 300W thermal overshoot
    
    // Check if the current target temperature is valid

    if (tempRed > targetTemp + HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
        status.heater = HEATERS_OFF;   // Update to use new enum
        publishSystemData();
        updateLEDs(status);
    }

    else if (tempRed < targetTemp - HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, LOW); // Changed: LOW = Relay ON

        // Use current sensor to determine actual heater state

        // status.heater = getHeaterState(currentReading);
        status.heater = BOTH_HEATERS_ON; // Update to use new enum
        updateLEDs(status);
        double currentReading = getCurrentReading(); // Get current reading for heater state analysis
        HeaterState heaterState = getHeaterState(currentReading);
        publishSystemData();

        // Enhanced heater failure detection with multi-heater support
        unsigned long now = millis(); // Declare now here so it's available for all conditions
        if (heaterState == ONE_HEATER_ON)
        {
            status.heater = ONE_HEATER_ON;
            updateLEDs(status);
        }

        if (heaterState == BOTH_HEATERS_BLOWN)
        {
            status.heater = BOTH_HEATERS_BLOWN;
            updateLEDs(status);
            // Total heater failure - both heaters not working
            if (firstRunTotalFailure)
            {
                firstRunTotalFailure = false;
                lastTotalFailureEmail = now;
                sendEmail("CRITICAL: Total Heater Failure",
                          "Both heaters have failed! Current: " + String(currentReading, 2) + "A. Immediate attention required!");
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
        // Heaters working properly - reset failure flags when current is detected
        firstRunSingleFailure = true;
        firstRunTotalFailure = true;
    } // End of else if (tempRed < targetTemp - HYSTERESIS) block
    else
    {
        // Temperature is within hysteresis band - maintain current state
        publishSystemData();
        updateLEDs(status);

    } // End of main temperature control if-else structure

} // End of updateHeaterControl function

// // Function to refresh the cached schedule values
// // Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
}
