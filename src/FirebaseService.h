// Expose Firebase initialization status for other modules
extern bool fbInitialized;
// ==================================================
// File: src/FirebaseService.h
// ==================================================

#pragma once
#include <Arduino.h>
#include "config.h"
#include <Firebase_ESP_Client.h>

// External Firebase data object
extern FirebaseData fbData;

struct SystemStatus;
void initFirebase(SystemStatus &status);
void handleFirebase(SystemStatus &status);
void pushTargetTempToFirebase(float targetTemp);
void fetchScheduleDataFromFirebase();
void publishFirebaseStatus(const char *status);
// In FirebaseService.h
// void pushTimeToFirebase(const String& amTime, const String& pmTime);
// void pushSensorValuesToFirebase();
// void checkAndPushTargetTemperature();
// void checkFirebaseTargetTemperatureChanges();
// void fetchControlValuesFromFirebase();
// void setControlValue(const char *path, float value);
// void setControlValue(const char *path, bool value);
// void setControlValue(const char *path, const char *value);

// Schedule data management
// bool isInitialScheduleFetched();
// void markInitialScheduleAsFetched();
