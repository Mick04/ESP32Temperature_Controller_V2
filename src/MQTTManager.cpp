//================
// MQTT Manager
//================

// Required for String, Serial, abs, isnan, etc.
#include <Arduino.h>
#include <math.h>
#include "Config.h"
#include "MQTTManager.h"
#include "TemperatureSensors.h"
#include "TimeManager.h"
#include "GetSchedule.h"
#include "HeaterControl.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "StatusLEDs.h"
#include "FirebaseService.h" // For Firebase status publishing and sensor data persistence
// Firebase status publishing helper is now implemented in FirebaseService.cpp

// Ensure status is available for LED updates
extern SystemStatus status;

// MQTT Client setup
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

// Global MQTT status
MQTTState mqttStatus = MQTT_STATE_DISCONNECTED;

// Client ID for MQTT connection
String clientId = "ESP32-TemperatureController-";

// Previous temperature values for change detection
static float prevTempRed = NAN;
static float prevTempBlue = NAN;
static float prevTempGreen = NAN;
static bool firstReading = true;

void publishSingleValue(const char *topic, float value)
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
        return;
    String payload = String(value, 2); // 2 decimal places
    if (mqttClient.publish(topic, payload.c_str()))
    {
        Serial.print("Published to ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(payload);
    }
    else
    {
        Serial.print("Failed to publish to ");
        Serial.println(topic);
    }
}

void publishSingleValue(const char *topic, int value)
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
        return;
    String payload = String(value);
    if (mqttClient.publish(topic, payload.c_str()))
    {
        Serial.print("Published to ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(payload);
    }
    else
    {
        Serial.print("Failed to publish to ");
        Serial.println(topic);
    }
}

void publishSingleValue(const char *topic, const char *value)
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
        return;
    if (mqttClient.publish(topic, value))
    {
        Serial.print("Published to ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(value);
    }
    else
    {
        Serial.print("Failed to publish to ");
        Serial.println(topic);
    }
}

void publishTimeData()
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
    {
        Serial.println("MQTT not connected, cannot publish time data");
        return;
    }
    Serial.println("Publishing time data to MQTT...");
    String timeStr = getFormattedTime();
    String dateStr = getFormattedDate();
    publishSingleValue(TOPIC_TIME, timeStr.c_str());
    publishSingleValue(TOPIC_DATE, dateStr.c_str());
}

MQTTState getMQTTStatus()
{
    return mqttStatus;
}

bool checkTemperatureChanges()
{
    float tempRed = getTemperature(0);   // Red sensor
    float tempBlue = getTemperature(1);  // Blue sensor
    float tempGreen = getTemperature(2); // Green sensor
    bool hasChanged = firstReading ||
                      (abs(tempRed - prevTempRed) > .5 && !isnan(tempRed)) ||
                      (abs(tempBlue - prevTempBlue) > .5 && !isnan(tempBlue)) ||
                      (abs(tempGreen - prevTempGreen) > .5 && !isnan(tempGreen)) ||
                      (isnan(tempRed) != isnan(prevTempRed)) ||
                      (isnan(tempBlue) != isnan(prevTempBlue)) ||
                      (isnan(tempGreen) != isnan(prevTempGreen));
    return hasChanged;
}

void onMQTTMessage(char *topic, unsigned char *payload, unsigned int length)
{
    // Serial.println("===================================");
    // Serial.println("");
    // Serial.println("🚨🚨🚨 CALLBACK ENTRY POINT HIT! 🚨🚨🚨");
    // Serial.flush();
    // Serial.println("🚨 MQTT CALLBACK TRIGGERED! 🚨");
    // Serial.print("Callback called with topic: ");
    // Serial.println(topic ? topic : "NULL");
    // Serial.print("Payload length: ");
    // Serial.println(length);
    // Serial.flush();
    String message = "";
    for (int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    // Serial.print("MQTT message received on topic: ");
    // Serial.println(topic);
    // Serial.print("*****************Message: ");
    // Serial.println(message);
    // Route schedule topics to handleScheduleUpdate
    String topicStr = String(topic);
    topicStr.trim();
    topicStr.toLowerCase();

    // Debug: Show what topic we received
    Serial.print("🔍 MQTT Topic received: '");
    Serial.print(topic);
    Serial.print("' -> normalized: '");
    Serial.print(topicStr);
    Serial.print("' Message: '");
    Serial.print(message);
    Serial.println("'");

    if (
        topicStr.endsWith("control/schedule") ||
        topicStr.endsWith("control/schedule/am/temperature") ||
        topicStr.endsWith("control/schedule/pm/temperature") ||
        topicStr.endsWith("control/schedule/am/time") ||
        topicStr.endsWith("control/schedule/pm/time") ||
        topicStr.endsWith("control/schedule/pm/hour") ||
        topicStr.endsWith("control/schedule/pm/minute") ||
        topicStr.endsWith("control/schedule/pm/enabled") ||
        topicStr.endsWith("control/schedule/pm/scheduledtime") ||
        // Add Firebase-style schedule topics
        topicStr.endsWith("schedule/amtemperature") ||
        topicStr.endsWith("schedule/pmtemperature") ||
        topicStr.endsWith("schedule/amscheduledtime") ||
        topicStr.endsWith("schedule/pmscheduledtime") ||
        topicStr.endsWith("schedule/amenabled") ||
        topicStr.endsWith("schedule/pmenabled"))
    {

        Serial.println("[DEBUG] Routing to handleScheduleUpdate...");
        handleScheduleUpdate(topic, message);
        Serial.println("✅ handleScheduleUpdate() called successfully");
    }
    else
    {
        // Serial.println("[DEBUG] Topic not routed to handleScheduleUpdate.");

        // Serial.println("===================================");
        // Serial.println("");
    }
}
void initMQTT()
{
    Serial.println("Initializing MQTT Manager...");

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected, cannot initialize MQTT");
        mqttStatus = MQTT_STATE_DISCONNECTED;
        // // If WiFi not connected, update LEDs to show MQTT disconnected
        // //updateLEDs(systemStatus);
        // delay(1000);
        return;
    }

    // Generate unique client ID to avoid conflicts with React app
    clientId = "ESP32_TempController_";
    clientId += String(WiFi.macAddress());
    clientId.replace(":", "");
    clientId += "_";
    clientId += String(millis()); // Add timestamp for absolute uniqueness

    Serial.print("🆔 MQTT Client ID: ");
    Serial.println(clientId);

    // Configure secure WiFi client for TLS connection
    wifiClientSecure.setInsecure(); // For testing - in production, use proper certificates

    // CRITICAL FIX: Set MQTT server and callback with enhanced registration
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT_TLS);

    // Set callback MULTIPLE times to ensure it sticks
    mqttClient.setCallback(onMQTTMessage);
    delay(50);
    mqttClient.setCallback(onMQTTMessage); // Double registration for safety

    Serial.println("✅ MQTT callback registered (double-confirmed)");

    // Set buffer size for larger messages and keepalive
    mqttClient.setBufferSize(512);
    mqttClient.setKeepAlive(60); // 60 second keepalive

    // Test callback registration immediately
    Serial.println("✅ MQTT callback registered");

    Serial.println("MQTT Manager initialized");
}

void handleMQTT()
{
    // Add debug to verify this function is being called
    static unsigned long lastDebugHandleMQTT = 0;
    if (millis() - lastDebugHandleMQTT > 60000) // Debug every 60 seconds
    {
        Serial.println("🔧 handleMQTT() function called");
        lastDebugHandleMQTT = millis();
    }

    // Don't proceed if WiFi is not connected
    if (WiFi.status() != WL_CONNECTED)
    {
        mqttStatus = MQTT_STATE_DISCONNECTED;
        return;
    }

    // Try to connect if not connected
    if (!mqttClient.connected())
    {
        mqttStatus = MQTT_STATE_CONNECTING;
        status.mqtt = mqttStatus;
        updateLEDs(status); // Show blue LED immediately while connecting
        if (connectToMQTT())
        {
            mqttStatus = MQTT_STATE_CONNECTED;
        }
        else
        {
            mqttStatus = MQTT_STATE_ERROR;
            // Rate limit connection attempts
            static unsigned long lastConnectAttempt = 0;
            if (millis() - lastConnectAttempt > 30000) // Try every 30 seconds
            {
                Serial.println("MQTT connection failed, will retry in 30 seconds");
                lastConnectAttempt = millis();
            }
        }
    }
    else
    {
        mqttStatus = MQTT_STATE_CONNECTED;
        // Keep the connection alive and process incoming messages
        // Call loop() more frequently for better message processing
        mqttClient.loop();

        static unsigned long lastLoopDebug = 0;
        if (millis() - lastLoopDebug > 30000) // Debug every 30 seconds
        {
            Serial.println("🔄 MQTT client loop() running, waiting for messages...");
            lastLoopDebug = millis();
        }
    }
}

bool connectToMQTT()
{
    Serial.print("Connecting to MQTT broker: ");
    Serial.println(MQTT_SERVER);
    const char *lwtTopic = "React/system/status";
    const char *lwtMessage = "offline";
    int lwtQos = 1;
    bool lwtRetain = true;

    // Attempt to connect with credentials
    if (mqttClient.connect(
            clientId.c_str(),
            MQTT_USER,
            MQTT_PASSWORD,
            lwtTopic,  // LWT topic
            lwtQos,    // LWT QoS
            lwtRetain, // LWT retain
            lwtMessage // LWT message
            ))
    {
        Serial.println("MQTT connected successfully!");
        // Publish MQTT and Firebase online status
        // publishSingleValue("React/mqtt/system/status", "online, true");
        // publishFirebaseStatus("online");

        // CRITICAL: Re-register callback after connection
        mqttClient.setCallback(onMQTTMessage);
        Serial.println("🔄 Callback re-registered after connection");

        // Subscribe to control topics (for receiving commands) with QoS 1 for guaranteed delivery
        Serial.println("📡 Subscribing to MQTT topics with QoS 1...");
        if (mqttClient.subscribe("React/control/+", 1))
        {
            Serial.println("✅ Subscribed to React/control/+ (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to React/control/+");
        }

        if (mqttClient.subscribe("React/commands/+", 1))
        {
            Serial.println("✅ Subscribed to React/commands/+ (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to React/commands/+");
        }

        // Subscribe to schedule topics to match React app format with QoS 1
        if (mqttClient.subscribe(TOPIC_CONTROL_AM_TEMP, 1))
        {
            Serial.println("✅ Subscribed to " TOPIC_CONTROL_AM_TEMP " (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to " TOPIC_CONTROL_AM_TEMP);
        }

        if (mqttClient.subscribe(TOPIC_CONTROL_PM_TEMP, 1))
        {
            Serial.println("✅ Subscribed to " TOPIC_CONTROL_PM_TEMP " (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to " TOPIC_CONTROL_PM_TEMP);
        }

        if (mqttClient.subscribe(TOPIC_CONTROL_AM_TIME, 1))
        {
            Serial.println("✅ Subscribed to " TOPIC_CONTROL_AM_TIME " (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to " TOPIC_CONTROL_AM_TIME);
        }

        if (mqttClient.subscribe(TOPIC_CONTROL_PM_TIME, 1))
        {
            Serial.println("✅ Subscribed to " TOPIC_CONTROL_PM_TIME " (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to " TOPIC_CONTROL_PM_TIME);
        }

        // Subscribe to React schedule topics that match Firebase structure
        if (mqttClient.subscribe("React/schedule/+", 1))
        {
            Serial.println("✅ Subscribed to React/schedule/+ (QoS 1)");
        }
        else
        {
            Serial.println("❌ Failed to subscribe to React/schedule/+");
        }

        // //Subscribe to JSON schedule updates with QoS 1
        // if (mqttClient.subscribe(TOPIC_CONTROL_SCHEDULE, 1))
        // {
        //     Serial.println("✅ Subscribed to " TOPIC_CONTROL_SCHEDULE " (QoS 1)");
        // }
        // else
        // {
        //     Serial.println("❌ Failed to subscribe to " TOPIC_CONTROL_SCHEDULE);
        // }

        // //Subscribe to additional React app topics with QoS 1
        // if (mqttClient.subscribe(TOPIC_CONTROL_AM_ENABLED, 1))
        // {
        //     Serial.println("✅ Subscribed to " TOPIC_CONTROL_AM_ENABLED " (QoS 1)");
        // }
        // if (mqttClient.subscribe(TOPIC_CONTROL_PM_ENABLED, 1))
        // {
        //     Serial.println("✅ Subscribed to " TOPIC_CONTROL_PM_ENABLED " (QoS 1)");
        // }
        // if (mqttClient.subscribe(TOPIC_CONTROL_PM_SCHEDULED_TIME, 1))
        // {
        //     Serial.println("✅ Subscribed to " TOPIC_CONTROL_PM_SCHEDULED_TIME " (QoS 1)");
        // }

        // Publish connection status
        publishSingleValue(TOPIC_STATUS, "online");

        // Add debug info about connection
        Serial.print("✅ MQTT Client State: ");
        Serial.println(mqttClient.state());
        Serial.println("📡 All subscriptions complete");

        // CRITICAL TEST: Send a self-test message to trigger callback with QoS 1
        Serial.println("🧪 Sending self-test message to verify callback...");
        delay(1000); // Give subscriptions time to settle

        // Call loop() multiple times to ensure message processing
        for (int i = 0; i < 5; i++)
        {
            mqttClient.loop();
            delay(100);
        }

        mqttClient.publish("React/commands/status", "SELF_TEST_CALLBACK", true); // Retained message with QoS
        Serial.println("📤 Self-test message sent with QoS, callback should trigger...");

        // Process the message immediately
        for (int i = 0; i < 10; i++)
        {
            mqttClient.loop();
            delay(50);
        }

        return true;
    }
    else
    {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" retrying...");
        // Publish MQTT and Firebase offline status
        publishSingleValue("React/mqtt/system/status", "offline, true");
        publishFirebaseStatus("offline");
        return false;
    }
}

void publishSensorData()
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
    {
        Serial.println("MQTT not connected, cannot publish sensor data");
        return;
    }

    // Get temperature readings
    float tempRed = getTemperature(0);   // Red sensor
    float tempBlue = getTemperature(1);  // Blue sensor
    float tempGreen = getTemperature(2); // Green sensor

    // Check if any temperature has changed (or this is the first reading)
    // Using 0.1°C as the minimum change threshold to avoid noise
    bool hasChanged = firstReading ||
                      (abs(tempRed - prevTempRed) > 0.1 && !isnan(tempRed)) ||
                      (abs(tempBlue - prevTempBlue) > 0.1 && !isnan(tempBlue)) ||
                      (abs(tempGreen - prevTempGreen) > 0.1 && !isnan(tempGreen)) ||
                      (isnan(tempRed) != isnan(prevTempRed)) ||
                      (isnan(tempBlue) != isnan(prevTempBlue)) ||
                      (isnan(tempGreen) != isnan(prevTempGreen));

    if (!hasChanged)
    {
        // Only print this message every 30 seconds to avoid spam
        static unsigned long lastNoChangeMessage = 0;
        if (millis() - lastNoChangeMessage > 30000)
        {
            Serial.println("📊 No significant temperature changes detected");
            lastNoChangeMessage = millis();
        }
        return;
    }

    Serial.println("🌡️  Temperature change detected, publishing sensor data to MQTT...");

    // Update previous values
    prevTempRed = tempRed;
    prevTempBlue = tempBlue;
    prevTempGreen = tempGreen;
    firstReading = false;

    // Calculate average temperature
    float avgTemp = 0;
    int validSensors = 0;

    if (!isnan(tempRed))
    {
        publishSingleValue(TOPIC_TEMP_RED, (float)(round(tempRed * 10) / 10.0)); // Round to 1 decimal place
        avgTemp += tempRed;
        validSensors++;
    }
    else
    {
        publishSingleValue(TOPIC_TEMP_RED, "ERROR");
    }

    if (!isnan(tempBlue))
    {
        publishSingleValue(TOPIC_TEMP_BLUE, (float)(round(tempBlue * 10) / 10.0)); // Round to 1 decimal place
        avgTemp += tempBlue;
        validSensors++;
    }
    else
    {
        publishSingleValue(TOPIC_TEMP_BLUE, "ERROR");
    }

    if (!isnan(tempGreen))
    {
        publishSingleValue(TOPIC_TEMP_GREEN, (float)(round(tempGreen * 10) / 10.0)); // Round to 1 decimal place
        avgTemp += tempGreen;
        validSensors++;
    }
    else
    {
        publishSingleValue(TOPIC_TEMP_GREEN, "ERROR");
    }

    // Also push sensor data to Firebase for dashboard initialization
    pushSensorDataToFirebase(tempRed, tempBlue, tempGreen);
    // float targetTemp = getTargetTemp();
    // if (!isnan(targetTemp))
    // {
    //     publishSingleValue("esp32/control/targetTemperature", (float)(round(targetTemp * 10) / 10.0)); // Round to 1 decimal place
    // }
    // else
    // {
    //     publishSingleValue(TOPIC_TEMP_GREEN, "ERROR");
    // }

    // Publish average temperature
    // if (validSensors > 0)
    // {
    //     avgTemp /= validSensors;
    //     publishSingleValue(TOPIC_TEMP_AVG, (float)(round(avgTemp * 10) / 10.0)); // Round to 1 decimal place
    //     Serial.print("📊 Average temperature: ");
    //     Serial.print(avgTemp);
    //     Serial.print("°C (from ");
    //     Serial.print(validSensors);
    //     Serial.println(" sensors)");
    // }
    // else
    // {
    //     publishSingleValue(TOPIC_TEMP_AVG, "ERROR");
    //     Serial.println("⚠️  No valid temperature sensors available");
    // }

    // Publish dummy current data (until current sensor is implemented)
    float dummyCurrent = random(0, 100) / 10.0; // Random current 0-10A
    publishSingleValue(TOPIC_CURRENT, dummyCurrent);

    // Also publish time and system data when temperature changes
    publishTimeData();
    publishSystemData();
}

// Ensure status is available for publishing heater status
extern SystemStatus status;

void publishSystemData()
{
    if (mqttStatus != MQTT_STATE_CONNECTED)
    {
        Serial.println("MQTT not connected, cannot publish system data");
        return;
    }

    // Serial.println("Publishing system data to MQTT...");

    static int prevRssi = INT_MIN;
    int rssi = WiFi.RSSI();
    if (rssi != prevRssi)
    {
        publishSingleValue(TOPIC_WIFI_RSSI, rssi);
        prevRssi = rssi;
    }

    // Publish uptime in hours, minutes, and seconds
    unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;
    char uptimeStr[16];
    snprintf(uptimeStr, sizeof(uptimeStr), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    publishSingleValue(TOPIC_UPTIME, uptimeStr);

    // Publish system status
    // publishSingleValue(TOPIC_STATUS, "online");

    // Publish heater status based on enum value
    const char *heaterStatus;
    switch (status.heater)
    {
    case HEATER_STARTUP:
        heaterStatus = "STARTUP";
        break;
    case HEATERS_OFF:
        heaterStatus = "OFF";
        break;
    case ONE_HEATER_ON:
    case BOTH_HEATERS_ON:
        heaterStatus = "ON";
        break;
    case BOTH_HEATERS_BLOWN:
        heaterStatus = "ERROR";
        break;
    default:
        heaterStatus = "UNKNOWN";
        break;
    }
    publishSingleValue("esp32/system/heater", heaterStatus);
}
