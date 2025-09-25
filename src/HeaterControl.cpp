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
// Heater control function
void updateHeaterControl(SystemStatus &status)
{

    Serial.println("******************Updating Heater Control...**************");
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
    Serial.println("Current Time: " + currentTime);
    Serial.println("Scheduled Time: " + scheduledTime);
    Serial.println("================================================");
    Serial.print("New Target Temp: ");
    Serial.println(newTargetTemp);
    Serial.print("Current Target Temp: ");
    Serial.println(targetTemp);
    Serial.print("Current Time: ");
    Serial.println(currentTime);
    Serial.print("Scheduled Time: ");
    Serial.println(scheduledTime);
    Serial.println("😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈😈");
    // Only update targetTemp at the scheduled time
    if (scheduledTime == currentTime)
    {
        targetTemp = newTargetTemp;
        Serial.println("👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺");
        Serial.println("==================================================");
        Serial.println("Debug output for target temperature update");
        Serial.print("newTargetTemp: ");
        Serial.println(newTargetTemp);
        Serial.print("Target temperature updated to: ");
        Serial.println(targetTemp);
        Serial.println("==================================================");
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
    Serial.print("Current Target Temperature: ");
    Serial.println(targetTemp);
    Serial.print("Current Red Sensor Temperature: ");
    Serial.println(tempRed);
    Serial.println("*******************************");
    if (tempRed > targetTemp + HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, LOW);
        status.heater = HEATER_OFF;
        publishSystemData();
        updateLEDs(status);
        Serial.println("🔥 Heater OFF - tempRed > targetTemp + HYSTERESIS");
    }
    else if (tempRed < targetTemp - HYSTERESIS)
    {
        digitalWrite(RELAY_PIN, HIGH);
        status.heater = HEATER_ON;
        publishSystemData();
        updateLEDs(status);
        if (!voltageSensor()) // Check if heater is drawing current (for safety)
        {
            //=========================
            static bool firstRunE_Mail = true;
            static unsigned long lastEmailTime = 0;
            unsigned long now = millis();
            if (firstRunE_Mail)
            {
                firstRunE_Mail = false;
                lastEmailTime = now;
                sendEmail("From ESP32: ", " Heater not working! ");
            }
            // Reset firstRunE_Mail after 30 minutes (1800000 ms)
            if (!firstRunE_Mail && (now - lastEmailTime >= 1800000UL))
            {
                firstRunE_Mail = true;
            }

            Serial.println("🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥");
            Serial.println("🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥 currentDetected");
            Serial.println("🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥🔥 Heater ON - tempRed < targetTemp - HYSTERESIS");
        }
        // else
        // {
        //     Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️ ");
        //     Serial.println("⚠️  Warning: Heater should be ON but no current detected! Possible issue with heater or wiring.");
        //     Serial.println("⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️⚠️");
        //     status.heater = HEATER_ERROR;
        //     publishSystemData();
        //     updateLEDs(status);
        // }

    } // else, keep current state
}

// // Function to refresh the cached schedule values
// // Call this whenever schedule data is updated via MQTT
void refreshScheduleCache()
{
    forceScheduleRefresh = true;
    Serial.println("🔄 Schedule cache refresh requested - will update on next heater control cycle");
}
