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
static bool firstRunLowCurrentFailure = true; // Separate flag for 1.91A case
static bool firstRunTotalFailure = true;
static unsigned long lastSingleFailureEmail = 0;
static unsigned long lastLowCurrentFailureEmail = 0; // Separate timer for 1.91A case
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
    const float HYSTERESIS = 0.25;                                    // degrees - Balanced for responsive control while preventing oscillation

    // Check if the current target temperature is valid

    if (tempRed > targetTemp + HYSTERESIS)
    // if (tempRed > targetTemp)
    {
        digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
        status.heater = HEATERS_OFF;   // Update to use new enum
        publishSystemData();
        updateLEDs(status);
        // Don't reset firstRunSingleFailure here - let it persist for 30-minute reminders
    }

    // else if (tempRed < targetTemp - HYSTERESIS)
    else if (tempRed < targetTemp)
    {
        digitalWrite(RELAY_PIN, LOW); // Changed: LOW = Relay ON

        // Use current sensor to determine actual heater state

        double currentReading = getCurrentReading(); // Get current reading for heater state analysis
        HeaterState heaterState = getHeaterState(currentReading);
        status.heater = heaterState; // Set status based on actual sensor reading
        updateLEDs(status);
        publishSystemData();
        Serial.println("♨️♨️♨️ Heater ON ♨️♨️♨️");
        Serial.print("Current reading: ");
        Serial.print(currentReading, 2);
        Serial.println(" A");
        Serial.print("Heater state: ");
        Serial.println(status.heater);
        Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
        // Enhanced heater failure detection with multi-heater support
        unsigned long now = millis(); // Declare now here so it's available for all conditions
        if (heaterState == ONE_HEATER_ON)
        {
            Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
            Serial.print("Current reading: ");
            Serial.print(currentReading, 2);
            Serial.println(" A");

            // Special case: Current around 1.91A indicates heater failure - send periodic emails
            if (currentReading >= 1.85 && currentReading <= 2.0)
            {
                if (firstRunLowCurrentFailure)
                {
                    firstRunLowCurrentFailure = false;
                    lastLowCurrentFailureEmail = now;
                    char message[200];
                    sprintf(message, "Heater failure detected. Current: %.2fA (abnormally low). Check heater connections and operation.", currentReading);
                    sendEmail("WARNING: Heater Failure - Low Current", message);
                }
                // Check if 30 minutes have passed since last email attempt
                else if (now - lastLowCurrentFailureEmail >= 1800000UL)
                {
                    lastLowCurrentFailureEmail = now; // Update timestamp regardless of email success
                    char message[200];
                    sprintf(message, "Heater failure detected. Current: %.2fA (abnormally low). Check heater connections and operation.", currentReading);
                    sendEmail("WARNING: Heater Failure - Low Current", message);
                }
            }
        }

        if (heaterState == BOTH_HEATERS_BLOWN)
        {
            // Total heater failure - both heaters not working
            Serial.println("♨️♨️♨️ Both heaters detected as BLOWN ♨️♨️♨️");
            Serial.print("Current reading: ");
            Serial.print(currentReading, 2);
            Serial.println(" A");

            // Debug timing information
            Serial.print("Time since last email: ");
            Serial.print((now - lastTotalFailureEmail) / 1000);
            Serial.println(" seconds");
            Serial.print("First run flag: ");
            Serial.println(firstRunTotalFailure ? "true" : "false");

            if (firstRunTotalFailure)
            {
                Serial.println("Sending FIRST total failure email");
                firstRunTotalFailure = false;
                lastTotalFailureEmail = now;
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("CRITICAL: Total Heater Failure", message);
            }
            // Check if 30 minutes have passed since last email attempt (1800000ms = 30min)
            else if (now - lastTotalFailureEmail >= 1800000UL)
            {
                Serial.println("30 minutes elapsed - sending repeat total failure email");
                lastTotalFailureEmail = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("CRITICAL: Total Heater Failure", message);
            }
            else
            {
                Serial.println("Waiting for 30-minute interval - no email sent");
            }
        }
        else if (heaterState == ONE_HEATER_ON && lastKnownState == BOTH_HEATERS_ON)
        {
            // Single heater failure - one heater stopped working
            if (firstRunSingleFailure)
            {
                firstRunSingleFailure = false;
                lastSingleFailureEmail = now;
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure", message);
            }
            // Check if 30 minutes have passed since last email attempt
            else if (now - lastSingleFailureEmail >= 1800000UL)
            {
                lastSingleFailureEmail = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure", message);
            }
        }

        // Update last known state for comparison
        if (heaterState != BOTH_HEATERS_BLOWN)
        {
            lastKnownState = heaterState;
        }
        // Reset failure flags only when both heaters are working properly
        if (heaterState == BOTH_HEATERS_ON)
        {
            firstRunSingleFailure = true;     // Only reset when both heaters working
            firstRunLowCurrentFailure = true; // Reset low current failure flag too
            firstRunTotalFailure = true;      // Always reset total failure flag when any heater current detected
        }
        // firstRunTotalFailure = true; // Always reset total failure flag when any heater current detected
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
