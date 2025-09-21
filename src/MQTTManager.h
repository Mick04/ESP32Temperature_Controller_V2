// ==================================================
// File: src/MQTTManager.h
// ==================================================

#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

// MQTT Configuration
#define MQTT_SERVER "ea53fbd1c1a54682b81526905851077b.s1.eu.hivemq.cloud"
#define MQTT_PORT_TLS 8883
#define MQTT_USER "ESP32FireBaseTortoise"
#define MQTT_PASSWORD "ESP32FireBaseHea1951Ter"

// MQTT Topics
// Sensor Data Topics (ESP32 publishes)
#define TOPIC_TEMP_RED "esp32/sensors/temperature/red"
#define TOPIC_TEMP_BLUE "esp32/sensors/temperature/blue"
#define TOPIC_TEMP_GREEN "esp32/sensors/temperature/green"
#define TOPIC_TARGET_TEMP "esp32/control/targetTemperature"
#define TOPIC_CURRENT "esp32/sensors/current"
#define TOPIC_TIME "esp32/system/time"
#define TOPIC_DATE "esp32/system/date"
#define TOPIC_STATUS "esp32/system/status"
#define TOPIC_WIFI_RSSI "esp32/system/wifi_rssi"
#define TOPIC_UPTIME "esp32/system/uptime"

// Control Topics (ESP32 subscribes to these)
//#define TOPIC_CONTROL_SCHEDULE "React/control/schedule"
#define TOPIC_CONTROL_AM_TEMP "React/control/schedule/am/temperature"
#define TOPIC_CONTROL_PM_TEMP "React/control/schedule/pm/temperature"
#define TOPIC_CONTROL_AM_TIME "React/control/schedule/am/time"
#define TOPIC_CONTROL_PM_TIME "React/control/schedule/pm/time"
//#define TOPIC_CONTROL_AM_ENABLED "React/control/schedule/am/enabled"
//#define TOPIC_CONTROL_PM_ENABLED "React/control/schedule/pm/enabled"
//#define TOPIC_CONTROL_PM_SCHEDULED_TIME "React/control/schedule/pm/scheduledTime"
//#define TOPIC_COMMANDS_STATUS "esp32/commands/status"

// Function declarations
void initMQTT();
void handleMQTT();
void publishSensorData();
void publishSystemData();
void publishTimeData();
bool connectToMQTT();
void onMQTTMessage(char *topic, byte *payload, unsigned int length);
void parseAndUpdateScheduleJson(const String &jsonMessage);
MQTTState getMQTTStatus();
void publishFirebaseStatus(const char *status);
void publishSingleValue(const char *topic, float value);
void publishSingleValue(const char *topic, int value);
void publishSingleValue(const char *topic, const char *value);
bool checkTemperatureChanges(); // Check if any temperature sensor values have changed

// Global MQTT status
extern MQTTState mqttStatus;

#endif // MQTTMANAGER_H
