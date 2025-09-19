// ==================================================
// File: src/FirebaseService.cpp
// ==================================================

#include "FirebaseService.h"
#include "config.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "TemperatureSensors.h"
#include "GetSchedule.h"

// External variable declarations for debugging
extern ScheduleData currentSchedule;
extern bool AmFlag;
extern int Hours;

// Provide the RTC and API key for RTDB
FirebaseData fbData; // Made non-static so other files can access it
static FirebaseConfig fbConfig;
static FirebaseAuth fbAuth;
static bool fbInitialized = false;
static bool initialScheduleFetched = false; // Track initial schedule fetch


void initFirebase(SystemStatus &status)
{
    Serial.println("👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺👺 Line 56 void initFirebase(SystemStatus &status)...");
    Serial.println(" ");
    // Initialize only when WiFi connected
    if (WiFi.status() != WL_CONNECTED)
    {
        status.firebase = FB_CONNECTING;
        Serial.println("WiFi not connected, cannot initialize Firebase");
        return;
    }

    Serial.println("Initializing Firebase...");

    // Debug print credentials (remove in production)
    Serial.println("Firebase credentials:");
    Serial.print("API Key: ");
    Serial.println(FIREBASE_API_KEY);
    Serial.print("Database URL: ");
    Serial.println(FIREBASE_DATABASE_URL);

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
        Serial.println("Firebase initialized and connected successfully");
        Serial.println("Test write successful");

        // Set device online status in Firebase (LWT-like)
        //setFirebaseOnlineStatus();

        // Now try to read back the data we just wrote
        Serial.println("Testing data retrieval...");
        if (Firebase.RTDB.getString(&fbData, "/test/connection"))
        {
            String retrievedValue = fbData.stringData();
            Serial.print("Retrieved value: ");
            Serial.println(retrievedValue);

            // Test reading a timestamp
            if (Firebase.RTDB.setTimestamp(&fbData, "/test/last_connection"))
            {
                Serial.println("Timestamp written successfully");
                if (Firebase.RTDB.getInt(&fbData, "/test/last_connection"))
                {
                    int timestamp = fbData.intData();
                    Serial.print("Connection timestamp: ");
                    Serial.println(timestamp);
                }
            }

            // // Immediately fetch schedule data from Firebase on startup
            // Serial.println("🚀 Fetching initial schedule data from Firebase...");
            // fetchScheduleDataFromFirebase();
            // initialScheduleFetched = true;
            // Serial.println("✅ Initial schedule fetch completed. Future updates will come via MQTT.");
        }
        else
        {
            Serial.println("Read test failed:");
            Serial.print("Error: ");
            Serial.println(fbData.errorReason());
        }
    }
    else
    {
        // Try a different approach - check if we can read instead
        Serial.println("Write failed, trying read test...");
        if (Firebase.ready())
        {
            fbInitialized = true;
            status.firebase = FB_CONNECTED;
            Serial.println("Firebase ready - assuming connection is good");
        }
        else
        {
            // Don't set fbInitialized = false here - we'll try again later
            status.firebase = FB_ERROR;
            Serial.println("Firebase initialization failed - will retry later");
            Serial.print("Error: ");
            Serial.println(fbData.errorReason());
            Serial.print("HTTP Code: ");
            Serial.println(fbData.httpCode());
        }
    }
}
void handleFirebase(SystemStatus &status)
{
    Serial.println(" ");
    Serial.println("Line 203 FirebaseService.cpp"); 
    Serial.println("🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡🤡 Line 198 handleFirebase...");
    Serial.println(" ");
    // === FIREBASE INITIALIZATION PHASE ===
    // If Firebase hasn't been initialized yet, try to initialize it
    if (!fbInitialized)
    {
        // Only attempt initialization if WiFi is connected
        if (WiFi.status() == WL_CONNECTED)
        {
            // Rate limit initialization attempts to avoid spam (every 30 seconds)
            static unsigned long lastInitAttempt = 0;
            if (millis() - lastInitAttempt > 30000)
            {
                Serial.println("****WiFi connected, initializing Firebase...");
                initFirebase(status); // Attempt Firebase initialization
                lastInitAttempt = millis();
            }
        }
        else
        {
            // WiFi not connected - set status and wait
            status.firebase = FB_CONNECTING;

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
        if (status.firebase != FB_CONNECTED)
        {
            // Status changed from disconnected to connected - log the recovery
            Serial.println("Firebase connected successfully");
        }
        status.firebase = FB_CONNECTED;
    }
    else
    {
        // Firebase connection is down
        status.firebase = FB_ERROR;
        Serial.println("Firebase connection lost");
    }
}