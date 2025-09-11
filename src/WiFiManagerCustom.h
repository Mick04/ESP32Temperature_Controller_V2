// ==================================================
// File: src/WiFiManagerCustom.h
// ==================================================

#pragma once
#include <Arduino.h>
#include "Config.h"
#include <WiFi.h> // Ensure WiFi library is included

struct SystemStatus;
void initWiFi(SystemStatus &status);
void handleWiFi(SystemStatus &status);