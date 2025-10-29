// ==================================================
// File: src/FirebaseService.cpp
// ==================================================

#include "FirebaseService.h"
#include "config.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "TemperatureSensors.h"
#include "GetSchedule.h"
#include "FirebaseService.h"
#include "config.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "TemperatureSensors.h"
#include "GetSchedule.h"
#include "MQTTManager.h" // For MQTT client access
#include "TimeManager.h" // For time formatting functions
#include "StatusLEDs.h"  // For LED status updates

// Forward declaration
void publishFirebaseStatus(const char *status);

// External variable declarations for debugging
extern ScheduleData currentSchedule;

// Forward declaration of MQTT client
extern PubSubClient mqttClient;

// Provide the RTC and API key for RTDB
FirebaseData fbData; // Made non-static so other files can access it
static FirebaseConfig fbConfig;
static FirebaseAuth fbAuth;
bool fbInitialized = false;
static bool initialScheduleFetched = false; // Track initial schedule fetch

void initFirebase(SystemStatus &status)
{
    // Initialize only when WiFi connected
    if (WiFi.status() != WL_CONNECTED)
    {
        status.firebase = FB_CONNECTING;
        updateLEDs(status);
        Serial.println("WiFi not connected, cannot initialize Firebase");
        return;
    }

    Serial.println("Initializing Firebase...");

    // Clear any previous configuration
    fbConfig = FirebaseConfig();
    fbAuth = FirebaseAuth();

    // Set the API key and database URL
    fbConfig.api_key = FIREBASE_API_KEY;
    fbConfig.database_url = FIREBASE_DATABASE_URL;

    // Set the host explicitly (extract from database URL)
    fbConfig.host = "esp32-heater-controler-6d11f-default-rtdb.europe-west1.firebasedatabase.app";

    // Set timeout and retry settings
    fbConfig.timeout.serverResponse = 15 * 1000;   // 15 seconds
    fbConfig.timeout.socketConnection = 15 * 1000; // 15 seconds

    // For anonymous authentication, we need to sign in after initialization
    Serial.println("Attempting anonymous authentication...");

    // Initialize Firebase first
    Firebase.begin(&fbConfig, &fbAuth);
    Firebase.reconnectWiFi(true);

    // Wait for initialization
    delay(3000);

    // Try anonymous authentication
    Serial.println("Signing in anonymously...");
    if (Firebase.signUp(&fbConfig, &fbAuth, "", ""))
    {
        Serial.println("Anonymous sign-up successful");
    }
    else
    {
        Serial.print("Anonymous sign-up failed: ");
        Serial.println("Check Firebase project settings for anonymous auth");
    }

    // Wait longer for authentication
    delay(2000);

    // Test the connection immediately
    Serial.println("Testing Firebase connection...");

    // Try to write a simple test value instead of just checking ready()
    if (Firebase.RTDB.setString(&fbData, "/test/connection", "esp32_test"))
    {
        fbInitialized = true;
        status.firebase = FB_CONNECTED;
        publishFirebaseStatus("online");

        // Now try to read back the data we just wrote
        Serial.println("Testing data retrieval...");
        if (Firebase.RTDB.getString(&fbData, "/test/connection"))
        {
            String retrievedValue = fbData.stringData();
            // Test reading a timestamp
            if (Firebase.RTDB.setTimestamp(&fbData, "/test/last_connection"))
            {
                Serial.println("Timestamp written successfully");
                if (Firebase.RTDB.getInt(&fbData, "/test/last_connection"))
                {
                    int timestamp = fbData.intData();
                }
            }

            // Immediately fetch schedule data from Firebase on startup
            fetchScheduleDataFromFirebase();
            initialScheduleFetched = true;
        }
    }
    else
    {
        // Try a different approach - check if we can read instead
        if (Firebase.ready())
        {
            fbInitialized = true;
            status.firebase = FB_CONNECTED;
        }
        else
        {
            // Don't set fbInitialized = false here - we'll try again later
            status.firebase = FB_ERROR;
        }
    }
}
void publishFirebaseStatus(const char *status)
{
    if (!fbInitialized)
    {
        return;
    }
    if (Firebase.RTDB.setString(&fbData, "ESP32/control", status))
    {
        if (mqttClient.connected())
        {
            mqttClient.publish("React/firebase/system/status", status, true);
        }
    }
}
void handleFirebase(SystemStatus &status)
{
    if (!fbInitialized)
    {
        // Only attempt initialization if WiFi is connected
        if (WiFi.status() == WL_CONNECTED)
        {
            updateLEDs(status);
            // Rate limit initialization attempts to avoid spam (every 30 seconds)
            static unsigned long lastInitAttempt = 0;
            if (millis() - lastInitAttempt > 30000)
            {
                initFirebase(status); // Attempt Firebase initialization
                lastInitAttempt = millis();
            }
        }
        else
        {
            // WiFi not connected - set status and wait
            status.firebase = FB_CONNECTING;
            updateLEDs(status);
            // Only print this message occasionally to avoid spam (every 5 seconds)
            static unsigned long lastWiFiMessage = 0;
            if (millis() - lastWiFiMessage > 5000)
            {
                Serial.println("WiFi not connected, waiting for connection...");
                lastWiFiMessage = millis();
            }
        }
        return; // Exit early if not initialized
    }

    // === CONNECTION MONITORING PHASE ===
    // Firebase is initialized, now monitor its connection health

    // Rate limit Firebase status checks to avoid excessive polling (every 10 seconds)
    static unsigned long lastStatusCheck = 0;
    if (millis() - lastStatusCheck < 10000)
    {
        return; // Skip this check cycle
    }
    lastStatusCheck = millis();

    // Check current Firebase connection status
    if (Firebase.ready())
    {
        // Firebase is ready and connected
        status.firebase = FB_CONNECTED;
        updateLEDs(status);
    }
    else
    {
        // Firebase connection is down
        status.firebase = FB_ERROR;
        updateLEDs(status);
    }
}
void pushTargetTempToFirebase(float targetTemp)
{
    if (!fbInitialized)
    {
        return;
    }
    Firebase.RTDB.setFloat(&fbData, "ESP32/control/target_temperature", targetTemp);
}

/**
 * Push current sensor readings to Firebase Realtime Database
 * This allows the dashboard to fetch initial values on load
 */
void pushSensorDataToFirebase(float tempRed, float tempBlue, float tempGreen)
{
    if (!fbInitialized)
    {
        return;
    }

    // Push red sensor data
    if (!isnan(tempRed))
    {
        Firebase.RTDB.setFloat(&fbData, "ESP32/control/sensors/tempRed", tempRed);
    }
    else
    {
        Firebase.RTDB.setString(&fbData, "ESP32/control/sensors/tempRed", "ERROR");
    }

    // Push blue sensor data
    if (!isnan(tempBlue))
    {
        Firebase.RTDB.setFloat(&fbData, "ESP32/control/sensors/tempBlue", tempBlue);
    }
    else
    {
        Firebase.RTDB.setString(&fbData, "ESP32/control/sensors/tempBlue", "ERROR");
    }

    // Push green sensor data
    if (!isnan(tempGreen))
    {
        Firebase.RTDB.setFloat(&fbData, "ESP32/control/sensors/tempGreen", tempGreen);
    }
    else
    {
        Firebase.RTDB.setString(&fbData, "ESP32/control/sensors/tempGreen", "ERROR");
    }

    // Store timestamp for when data was last updated
    String timestamp = getFormattedTime();
    Firebase.RTDB.setString(&fbData, "ESP32/control/sensors/lastUpdated", timestamp);
}

/**
 * Push WiFi RSSI values to Firebase Realtime Database
 * Includes signal strength classification for dashboard
 */
void pushRSSIToFirebase(long rssi)
{
    if (!fbInitialized)
    {
        return;
    }

    // Push raw RSSI value
    Firebase.RTDB.setInt(&fbData, "ESP32/control/wifi/rssi", rssi);

    // Classify signal strength and push as string for easy dashboard display
    String signalQuality;
    if (rssi > -50)
    {
        signalQuality = "Excellent";
    }
    else if (rssi > -60)
    {
        signalQuality = "Very Good";
    }
    else if (rssi > -70)
    {
        signalQuality = "Good";
    }
    else if (rssi > -80)
    {
        signalQuality = "Low";
    }
    else
    {
        signalQuality = "Very Low";
    }

    Firebase.RTDB.setString(&fbData, "ESP32/control/wifi/signalQuality", signalQuality);

    // Store timestamp for when RSSI was last updated
    String timestamp = getFormattedTime();
    Firebase.RTDB.setString(&fbData, "ESP32/control/wifi/lastUpdated", timestamp);
}
