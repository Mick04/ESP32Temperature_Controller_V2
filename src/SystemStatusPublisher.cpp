#include <Arduino.h>
#include <Firebase_ESP_Client.h>

extern bool fbInitialized;
extern FirebaseData fbData;

// Publishes a heartbeat timestamp to Firebase for last-seen detection
void publishFirebaseHeartbeat()
{
    if (fbInitialized)
    {
        unsigned long now = millis() / 1000;
        Firebase.RTDB.setInt(&fbData, "/system/last_seen", now);
    }
}
#include "SystemStatusPublisher.h"
#include "FirebaseService.h"
#include "MQTTManager.h"
#include <Firebase_ESP_Client.h>

extern PubSubClient mqttClient;
extern bool fbInitialized;
extern FirebaseData fbData;

void publishAllSystemStatus(const SystemStatus &status)
{
    // WiFi status
    const char *wifiStatus = (status.wifi == CONNECTED) ? "online" : "offline";
    if (mqttClient.connected())
    {
        mqttClient.publish("React/wifi/system/status", wifiStatus, true);
    }
    // MQTT status
    const char *mqttStatus = (status.mqtt == MQTT_STATE_CONNECTED) ? "online" : "offline";
    if (mqttClient.connected())
    {
        mqttClient.publish("React/mqtt/system/status", mqttStatus, true);
    }
    // Firebase status
    const char *fbStatus = (status.firebase == FB_CONNECTED) ? "online" : "offline";
    if (mqttClient.connected())
    {
        mqttClient.publish("React/firebase/system/status", fbStatus, true);
    }
    // Also update Firebase RTDB
    if (fbInitialized)
    {
        Firebase.RTDB.setString(&fbData, "/system/wifi_status", wifiStatus);
        Firebase.RTDB.setString(&fbData, "/system/mqtt_status", mqttStatus);
        Firebase.RTDB.setString(&fbData, "/system/firebase_status", fbStatus);
    }
}
