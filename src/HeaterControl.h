// Getter for current target temperature
float getTargetTemp();
// ================================
// Heater Control Functions
// ================================

#ifndef HEATERCONTROL_H
#define HEATERCONTROL_H

#include <Arduino.h>
#include "Config.h"
#include "TimeManager.h"

// Function declarations
void updateHeaterControl(SystemStatus &status);
void refreshScheduleCache(); // Force refresh of cached schedule values
void getTime();
void publishSystemData();
#endif // HEATERCONTROL_H
