//=====================================
// SCT-013-020 Current Monitor
//=====================================
// Measures current draw from heater to verify operation
// Uses EmonLib library for RMS current calculation
// Enhanced to detect multiple heater configurations
//=====================================
#include <Arduino.h>
#include "EmonLib.h"
#include <SPIFFS.h>
#include "Config.h"

EnergyMonitor emon1;

// Get current reading for detailed heater analysis
double getCurrentReading()
{
    // Initialize sensor on first call only (one-time setup)
    static bool initialized = false;
    if (!initialized)
    {
        emon1.current(CURRENT_SENSOR_PIN, CALIBRATION_CONSTANT);
        initialized = true;
    }

    // Read RMS current
    double Irms = emon1.calcIrms(SAMPLES_PER_READING);
    Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
    Serial.print("Raw Irms reading: ");
    Serial.print(Irms);
    Serial.println(" A");
    Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
    // Apply baseline offset correction
    Irms -= BASELINE_OFFSET;
    if (Irms < 0)
        Irms = 0.0;

    // Filter noise
    if (Irms < NOISE_THRESHOLD)
    {
        Irms = 0.0;
    }

    return Irms;
}

// Get detailed heater status based on current draw - Updated for 2×100W heaters
// Current readings: 1×100W = ~2.3A, 2×100W = ~3.8A
//
// Previous 150W heater configuration (for reference if reverting):
// Readings: 1×150W = ~3.0A, 2×150W = ~5.9A
// Old thresholds:
//   if (current >= 1.5 && current <= 3.0) return ONE_HEATER_ON;   // One 150W heater
//   if (current > 3.5) return BOTH_HEATERS_ON;                    // Both 150W heaters
HeaterState getHeaterState(double current)
{
    if (current < 0.45)
    {
        return BOTH_HEATERS_BLOWN; // No current detected
    }
    else if (current >= 0.46 && current < 1.5)
    {
        return BOTH_HEATERS_BLOWN; // Very low current - both heaters severely degraded/failing
    }
    else if (current >= 1.6 && current <= 3.0)
    {
        return ONE_HEATER_ON; // ~2.3A = One 100W heater
    }
    else if (current >= 3.1)
    {
        return BOTH_HEATERS_ON; // ~3.8A = Both 100W heaters
    }
    else
    {
        return HEATERS_OFF; // Between thresholds - uncertain state
    }
}
