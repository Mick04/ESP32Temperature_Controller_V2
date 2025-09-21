// Publishes a heartbeat timestamp to Firebase for last-seen detection
void publishFirebaseHeartbeat();
#pragma once
#include "Config.h"
#include "FirebaseService.h"
#include "MQTTManager.h"

struct SystemStatus;

// Publishes WiFi, MQTT, and Firebase status to MQTT and Firebase
void publishAllSystemStatus(const SystemStatus &status);
