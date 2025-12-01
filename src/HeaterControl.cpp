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
    //***************************************
    // Keep this code No. 1
    //***************************************
    if (tempRed > targetTemp + HYSTERESIS)
    // if (tempRed > targetTemp)
    {

        digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
        status.heater = HEATERS_OFF;   // Update to use new enum
        publishSystemData();

        // remove after testing start
        // double currentReading = getCurrentReading(); // Get current voltage reading for heater state analysi
        // HeaterState heaterState = getHeaterState(currentReading);
        // status.heater = heaterState;
        Serial.println("");
        Serial.println("line 85 updateLEDs)");
        updateLEDs(status);
        Serial.println("");
        Serial.println("❄️❄️❄️ Heater OFF ❄️❄️❄️");
        // Serial.println("Current reading: ");
        // Serial.print(getCurrentReading(), 2);
        // Serial.println(" A");
        Serial.println("heaterState");
        Serial.println(status.heater);
        Serial.println("❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️❄️");
        Serial.println("");

        // remove after testing end

        // Use current sensor to determine actual heater state
        // No3

        // Don't reset first_Run_Single_Failure here - let it persist for 30-minute reminders
    }
    //************************************
    // No2
    else if (tempRed < targetTemp)
    {
        digitalWrite(RELAY_PIN, LOW);                // Changed: LOW = Relay ON
        double currentReading = getCurrentReading(); // Get current voltage reading for heater state analysis
        HeaterState heaterState = getHeaterState(currentReading);
        status.heater = heaterState;
        Serial.println("line 118 updateLEDs)");
        updateLEDs(status);

        Serial.println("");
        Serial.println("♨️♨️♨️ Heater ON ♨️♨️♨️");
        Serial.println("Current reading: ");
        Serial.print(getCurrentReading(), 2);
        Serial.println(" A");
        Serial.println("");
        Serial.println("heaterState");
        Serial.println(heaterStateToString(heaterState)); // Shows: "ONE_HEATER_ON"
        Serial.println("");
        Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
        Serial.println("");

        // Use current sensor to determine actual heater state
        // No3

        Serial.println("");
        Serial.println("");
        Serial.println("🍎🍎🍎 Heater State Analysis🍎🍎🍎");
        Serial.println("heaterState");
        Serial.println(heaterStateToString(heaterState)); // Shows: "ONE_HEATER_ON"
        Serial.println("");
        Serial.println("");

        publishSystemData();

        unsigned long now = millis(); // Declare now here so it's available for all conditions
        if (heaterState == ONE_HEATER_ON)
        {
            Serial.println("");
            Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
            Serial.print("now - last_Single_Heater_Alert_Email: ");
            Serial.println(now - last_Single_Heater_Alert_Email/60);
            // Serial.println(" A");
            Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
            Serial.println("");
            if (first_Run_Single_Heater_Alert)
            {
                Serial.println("Sending FIRST low current failure email");
                first_Run_Single_Heater_Alert = false;
                //first_Run_Total_Failure = now; // Reset total failure flag as at least one heater is working
                last_Single_Heater_Alert_Email = now;
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure Attention needed", message);
            }

            // Check if 30 minutes have passed since last email attempt
            else if (now - last_Single_Heater_Alert_Email >= 60000UL)//1800000UL
            {
                Serial.println("1 minute elapsed - sending repeat low current failure email");
                last_Single_Heater_Alert_Email = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
                sendEmail("WARNING: Single Heater Failure Attention needed", message);
            }
        }
        if (heaterState == BOTH_HEATERS_BLOWN)
        {
            Serial.println("");
            Serial.println("♨️♨️♨️ Both heaters detected as BLOWN ♨️♨️♨️");
            Serial.print("now - last_Total_Failure_Email: ");
            Serial.print((now - last_Total_Failure_Email)/60);
            // Serial.println(" A");
            Serial.println("");
            if (first_Run_Total_Failure)
            {
                Serial.println("Sending FIRST total failure email");
                first_Run_Total_Failure = false;
                last_Total_Failure_Email = now;
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("Immediate attention required! CRITICAL: Total Heater Failure", message);
            }

            else if (now - last_Total_Failure_Email >= 3600000UL)
            {
                Serial.println("30 minutes elapsed - sending repeat total failure email");
                last_Total_Failure_Email = now; // Update timestamp regardless of email success
                char message[200];
                sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
                sendEmail("Immediate attention required! CRITICAL: Total Heater Failure", message);
            }
        }
        if (heaterState == BOTH_HEATERS_ON)
        {
            first_Run_Single_Heater_Alert = true; // Reset here, not in the email block
        }
    }
    else
    {
        // Temperature is within hysteresis band - maintain current state
        publishSystemData();
        Serial.println("line 210 updateLEDs)");
        updateLEDs(status);

    } // End of main temperature control if-else structure
} // End of updateHeaterControl function
  // Check if the current target temperature is valid

// if (tempRed > targetTemp + HYSTERESIS)
// // if (tempRed > targetTemp)
// {
//     digitalWrite(RELAY_PIN, HIGH); // Changed: HIGH = Relay OFF
//     status.heater = HEATERS_OFF;   // Update to use new enum
//     publishSystemData();
//     updateLEDs(status);
//     // Don't reset first_Run_Single_Failure here - let it persist for 30-minute reminders
// }

// // else if (tempRed < targetTemp - HYSTERESIS)
// else if (tempRed < targetTemp)
// {
//     digitalWrite(RELAY_PIN, LOW); // Changed: LOW = Relay ON

//     // Use current sensor to determine actual heater state

//     double currentReading = getCurrentReading(); // Get current reading for heater state analysis
//     HeaterState heaterState = getHeaterState(currentReading);
//     status.heater = heaterState; // Set status based on actual sensor reading
//     updateLEDs(status);
//     publishSystemData();
//     Serial.println("");
//     Serial.println("");
//     Serial.println("");
//     Serial.println("♨️♨️♨️ Heater ON ♨️♨️♨️");
//     Serial.print("Current reading: ");
//     Serial.print(currentReading, 2);
//     Serial.println(" A");
//     Serial.print("Heater state: ");
//     Serial.println(status.heater);
//     Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
//     Serial.println("");
//     Serial.println("");
//     Serial.println("");
//     // Enhanced heater failure detection with multi-heater support
//     unsigned long now = millis(); // Declare now here so it's available for all conditions
//     if (heaterState == ONE_HEATER_ON)
//     {
//         Serial.println("");
//         Serial.println("");
//         Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
//         Serial.print("Current reading: ");
//         Serial.print(currentReading, 2);
//         Serial.println(" A");
//         Serial.println("♨️♨️♨️ One heater detected as ON ♨️♨️♨️");
//         Serial.println("");
//         Serial.println("");
//         Serial.println("");

//         // Special case: Current around 1.91A indicates heater failure - send periodic emails
//         // if (currentReading >= 1.85 && currentReading <= 2.0)
//         // {
//         //     // Debug timing information for low current failure
//         //     Serial.print("⚠️ Low current failure detected. Time since last email: ");
//         //     Serial.print((now - last_Single_Heater_Alert_Email) / 1000);
//         //     Serial.println(" seconds");
//         //     Serial.print("First run flag: ");
//         //     Serial.println(first_Run_Single_Heater_Alert ? "true" : "false");

//         if (first_Run_Single_Heater_Alert)
//         {
//             Serial.println("Sending FIRST low current failure email");
//             first_Run_Single_Heater_Alert = false;
//             last_Single_Heater_Alert_Email = now;
//             char message[200];
//             sprintf(message, "Heater failure detected. Current: %.2fA (abnormally low). Check heater connections and operation.", currentReading);
//             sendEmail("WARNING: Heater Failure - Low Current", message);
//         }
//         // Check if 30 minutes have passed since last email attempt
//         else if (now - last_Single_Heater_Alert_Email >= 60000UL)
//         {
//             Serial.println("30 minutes elapsed - sending repeat low current failure email");
//             last_Single_Heater_Alert_Email = now; // Update timestamp regardless of email success
//             char message[200];
//             sprintf(message, "Heater failure detected. Current: %.2fA (abnormally low). Check heater connections and operation.", currentReading);
//             sendEmail("WARNING: Heater Failure - Low Current", message);
//         }
//         else
//         {
//             Serial.println("Waiting for 30-minute interval - no low current email sent");
//         }
//         //  }
//         // Note: first_Run_Single_Heater_Alert flag is only reset when BOTH_HEATERS_ON (below)
//     }

//     if (heaterState == BOTH_HEATERS_BLOWN)
//     {
//         // Total heater failure - both heaters not working
//         Serial.println("♨️♨️♨️ Both heaters detected as BLOWN ♨️♨️♨️");
//         Serial.print("Current reading: ");
//         Serial.print(currentReading, 2);
//         Serial.println(" A");

//         // Debug timing information
//         Serial.print("Time since last email: ");
//         Serial.print((now - last_Total_Failure_Email) / 1000);
//         Serial.println(" seconds");
//         Serial.print("First run flag: ");
//         Serial.println(first_Run_Total_Failure ? "true" : "false");

//         if (first_Run_Total_Failure)
//         {
//             Serial.println("Sending FIRST total failure email");
//             first_Run_Total_Failure = false;
//             last_Total_Failure_Email = now;
//             char message[200];
//             sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
//             sendEmail("CRITICAL: Total Heater Failure", message);
//         }
//         // Check if 30 minutes have passed since last email attempt (1800000ms = 30min)
//         else if (now - last_Total_Failure_Email >= 1800000UL)
//         {
//             Serial.println("30 minutes elapsed - sending repeat total failure email");
//             last_Total_Failure_Email = now; // Update timestamp regardless of email success
//             char message[200];
//             sprintf(message, "Both heaters have failed! Current: %.2fA. Immediate attention required!", currentReading);
//             sendEmail("CRITICAL: Total Heater Failure", message);
//         }
//         else
//         {
//             Serial.println("Waiting for 30-minute interval - no email sent");
//         }
//     }
//     else if (heaterState == ONE_HEATER_ON && lastKnownState == BOTH_HEATERS_ON)
//     {
//         // Single heater failure - one heater stopped working
//         if (first_Run_Single_Failure)
//         {
//             first_Run_Single_Failure = false;
//             last_Single_Failure_Email = now;
//             char message[200];
//             sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
//             sendEmail("WARNING: Single Heater Failure", message);
//         }
//         // Check if 30 minutes have passed since last email attempt
//         else if (now - last_Single_Failure_Email >= 1800000UL)
//         {
//             last_Single_Failure_Email = now; // Update timestamp regardless of email success
//             char message[200];
//             sprintf(message, "One heater has failed. Current: %.2fA (expected ~4.6A). System still operational but reduced efficiency.", currentReading);
//             sendEmail("WARNING: Single Heater Failure", message);
//         }
//     }

//     // Update last known state for comparison
//     if (heaterState != BOTH_HEATERS_BLOWN)
//     {
//         lastKnownState = heaterState;
//     }
//     // Reset failure flags only when both heaters are working properly
//     if (heaterState == BOTH_HEATERS_ON)
//     {
//         // Reset all failure flags when both heaters confirmed working
//         if (!first_Run_Single_Failure || !first_Run_Single_Heater_Alert || !first_Run_Total_Failure)
//         {
//             Serial.println("✅ Both heaters working properly - resetting all failure flags");
//         }
//         first_Run_Single_Failure = true;      // Reset when both heaters working
//         first_Run_Single_Heater_Alert = true; // Reset low current failure flag when both heaters working
//         first_Run_Total_Failure = true;       // Reset total failure flag when both heaters working
//     }
//     // first_Run_Total_Failure = true; // Always reset total failure flag when any heater current detected
// } // End of else if (tempRed < targetTemp - HYSTERESIS) block

// // Function to refresh the cached schedule values
// // Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
}
