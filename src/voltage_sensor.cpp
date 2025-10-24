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

// Check if heater is actually drawing current (for safety verification)
bool voltageSensor()
{
    Serial.println("🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎");
    Serial.println("==============================");
    Serial.println("SCT-013-020 Current Sensor");
    Serial.println("==============================");
    Serial.println("🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎🍎");
    // Initialize sensor on first call only (one-time setup)
    static bool initialized = false;
    if (!initialized)
    {
        emon1.current(CURRENT_SENSOR_PIN, CALIBRATION_CONSTANT); // Setup SCT-013-020 with calibrated values
        initialized = true;
    }

    // Read RMS current from SCT-013-020 sensor (takes 5000 samples for accuracy)
    double Irms = emon1.calcIrms(SAMPLES_PER_READING);

    // Apply baseline offset correction to eliminate ADC drift when no current flows
    Irms -= BASELINE_OFFSET; // Subtract 1.75A offset
    if (Irms < 0)
        Irms = 0.0; // Ensure no negative readings

    // Filter out electrical noise below threshold
    if (Irms < NOISE_THRESHOLD) // Ignore readings below 0.1A
    {
        Irms = 0.0;
    }

    // Check if heater is drawing current (simplified for safety check)
    bool currentDetected = (Irms > HEATER_ON_THRESHOLD); // Current > 0.45A indicates heater is on

    // Debug output for safety verification can be removed in production
    if (ENABLE_DEBUG_OUTPUT)
    {
        Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
        Serial.println("Safety Check - Current: ");
        Serial.print(Irms, 3);
        Serial.println("A, Heater Detected: ");
        Serial.println(currentDetected ? "YES" : "NO");
        Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
    }

    return currentDetected; // Return true if heater is drawing current, false if not
}

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

// Get detailed heater status based on current draw
HeaterState getHeaterState(double current)
{
    if (current < 0.45)
    {
        return BOTH_HEATERS_BLOWN;
    }
    else if (current >= 1.5 && current <= 3.0)
    {
        return ONE_HEATER_ON;
    }
    else if (current > 3.5)
    {
        return BOTH_HEATERS_ON;
    }
    // else
    // {
    //     return HEATER_UNKNOWN;
    // }
}
