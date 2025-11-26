// ==================================================
// File: src/FirebaseService.cpp
// ==================================================

#include "FirebaseService.h"
#include "config.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
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

// New function: Store sensor data with timestamp for historical charts
void pushSensorDataToFirebaseHistory(float tempRed, float tempBlue, float tempGreen)
{
    if (!fbInitialized)
    {
        return;
    }

    // Get current timestamp in ISO format
    time_t now = time(nullptr);
    char basePath[100];
    char latestPointerPath[100];

    if (now < 1000000000)
    { // If time not set properly, use millis as fallback
        snprintf(basePath, sizeof(basePath), "ESP32/history/sensors/T%lu", millis());

        // Always update the latest pointer for React app discovery
        snprintf(latestPointerPath, sizeof(latestPointerPath), "ESP32/history/latest");
        Firebase.RTDB.setString(&fbData, latestPointerPath, basePath);

        if (!isnan(tempRed))
        {
            char redPath[120];
            snprintf(redPath, sizeof(redPath), "%s/tempRed", basePath);
            Firebase.RTDB.setFloat(&fbData, redPath, tempRed);
        }
        if (!isnan(tempBlue))
        {
            char bluePath[120];
            snprintf(bluePath, sizeof(bluePath), "%s/tempBlue", basePath);
            Firebase.RTDB.setFloat(&fbData, bluePath, tempBlue);
        }
        if (!isnan(tempGreen))
        {
            char greenPath[120];
            snprintf(greenPath, sizeof(greenPath), "%s/tempGreen", basePath);
            Firebase.RTDB.setFloat(&fbData, greenPath, tempGreen);
        }

        char timeStringPath[120];
        char chartDataPath[120];
        snprintf(timeStringPath, sizeof(timeStringPath), "%s/timeString", basePath);
        snprintf(chartDataPath, sizeof(chartDataPath), "%s/chartData", basePath);

        String formattedTime = getFormattedTime();
        Firebase.RTDB.setString(&fbData, timeStringPath, formattedTime);

        // Add chart-optimized data structure for fallback case
        char chartDataJson[200];
        snprintf(chartDataJson, sizeof(chartDataJson),
                 "{\"x\":\"%s\",\"red\":%.2f,\"blue\":%.2f,\"green\":%.2f,\"millis\":%lu}",
                 formattedTime.c_str(),
                 isnan(tempRed) ? 0.0 : tempRed,
                 isnan(tempBlue) ? 0.0 : tempBlue,
                 isnan(tempGreen) ? 0.0 : tempGreen,
                 millis());
        Firebase.RTDB.setString(&fbData, chartDataPath, chartDataJson);
    }
    else
    {
        Serial.println("✅ Using NTP time for organized daily structure");
        // Use organized daily structure for better performance with 3 months of data
        struct tm *timeinfo = gmtime(&now);
        char dateKey[16];
        char timeKey[16];
        char isoTimestamp[32];
        char basePath[100];

        // Create date-based organization: YYYY-MM-DD/HH-MM-SS
        strftime(dateKey, sizeof(dateKey), "%Y-%m-%d", timeinfo);
        strftime(timeKey, sizeof(timeKey), "%H-%M-%S", timeinfo);
        strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

        // Organized path: ESP32/history/daily/2024-11-07/10-30-00/
        snprintf(basePath, sizeof(basePath), "ESP32/history/daily/%s/%s", dateKey, timeKey);

        // Update latest pointer for React app discovery
        snprintf(latestPointerPath, sizeof(latestPointerPath), "ESP32/history/latest");
        Firebase.RTDB.setString(&fbData, latestPointerPath, basePath);

        if (!isnan(tempRed))
        {
            char redPath[120];
            snprintf(redPath, sizeof(redPath), "%s/tempRed", basePath);
            Firebase.RTDB.setFloat(&fbData, redPath, tempRed);
        }
        if (!isnan(tempBlue))
        {
            char bluePath[120];
            snprintf(bluePath, sizeof(bluePath), "%s/tempBlue", basePath);
            Firebase.RTDB.setFloat(&fbData, bluePath, tempBlue);
        }
        if (!isnan(tempGreen))
        {
            char greenPath[120];
            snprintf(greenPath, sizeof(greenPath), "%s/tempGreen", basePath);
            Firebase.RTDB.setFloat(&fbData, greenPath, tempGreen);
        }

        // Store both Unix timestamp and ISO string for easy querying
        char timestampPath[120];
        char isoPath[120];
        char chartDataPath[120];
        snprintf(timestampPath, sizeof(timestampPath), "%s/timestamp", basePath);
        snprintf(isoPath, sizeof(isoPath), "%s/iso", basePath);
        snprintf(chartDataPath, sizeof(chartDataPath), "%s/chartData", basePath);

        Firebase.RTDB.setInt(&fbData, timestampPath, now);
        Firebase.RTDB.setString(&fbData, isoPath, isoTimestamp);

        // Add chart-optimized data structure
        char chartDataJson[200];
        snprintf(chartDataJson, sizeof(chartDataJson),
                 "{\"x\":\"%s\",\"red\":%.2f,\"blue\":%.2f,\"green\":%.2f}",
                 isoTimestamp,
                 isnan(tempRed) ? 0.0 : tempRed,
                 isnan(tempBlue) ? 0.0 : tempBlue,
                 isnan(tempGreen) ? 0.0 : tempGreen);
        Firebase.RTDB.setString(&fbData, chartDataPath, chartDataJson);
    }
}

// Function to periodically store historical data (call this every 5 minutes)
void storeHistoricalDataIfNeeded(float tempRed, float tempBlue, float tempGreen)
{
    static unsigned long lastHistoricalStore = 0;
    static unsigned long lastCleanup = 0;
    const unsigned long HISTORICAL_INTERVAL = 600000; // 10 minutes for testing (change back to 300000 for 5 minutes)
    const unsigned long CLEANUP_INTERVAL = 604800000; // 7 days in milliseconds (weekly cleanup)

    unsigned long now = millis();

    // Store historical data every 5 minutes
    if (now - lastHistoricalStore >= HISTORICAL_INTERVAL)
    {
        pushSensorDataToFirebaseHistory(tempRed, tempBlue, tempGreen);
        lastHistoricalStore = now;
    }
    else
    {
        unsigned long remainingTime = HISTORICAL_INTERVAL - (now - lastHistoricalStore);
    }

    // Clean up old data once per week (removes data older than 3 months)
    if (now - lastCleanup >= CLEANUP_INTERVAL)
    {
        cleanupOldHistoricalData();
        lastCleanup = now;
    }
}

// Cleanup function (removes data older than 3 months = 90 days)
void cleanupOldHistoricalData()
{
    if (!fbInitialized)
    {
        return;
    }
    time_t now = time(nullptr);
    if (now < 1000000000)
    {
        return;
    }

    // Calculate cutoff date (90 days ago for 3-month retention)
    time_t cutoff = now - (90 * 24 * 60 * 60);
    struct tm *cutoffInfo = gmtime(&cutoff);
    char cutoffDate[16];
    strftime(cutoffDate, sizeof(cutoffDate), "%Y-%m-%d", cutoffInfo);

    // For the daily structure ESP32/history/daily/YYYY-MM-DD/
    // we would need to:
    // 1. Query all date folders under ESP32/history/daily/
    // 2. Compare each YYYY-MM-DD with cutoffDate
    // 3. Delete entire daily folders older than 90 days

    // This is more efficient than the old structure since we can
    // delete entire days at once rather than individual entries
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
