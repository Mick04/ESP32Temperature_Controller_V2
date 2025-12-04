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
#include "FirebaseService.h"

// Add these static variables at the top
//for periodic Irms updates
static unsigned long lastIrmsUpdate = 0;
static double lastIrmsReading = 0;

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
#if DEBUG_SERIAL
    Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
    Serial.print("Raw Irms reading: ");
    Serial.print(Irms);
    Serial.println(" A");
    Serial.println("♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️♨️");
#endif
    // Store last reading for Firebase updates
lastIrmsReading = Irms;

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

HeaterState getHeaterState(double current)
{
    static HeaterState lastState = BOTH_HEATERS_ON; // Remember last state
    static const double HYSTERESIS = 0.2; // 0.2A deadband to prevent flickering
    
    // Determine raw state first
    HeaterState rawState;
    
    if (current < 1.5)
    {
        rawState = BOTH_HEATERS_BLOWN;
    }
    else if (current >= 1.5 && current <= 2.8)
    {
        rawState = ONE_HEATER_ON;
    }
    else if (current > 2.8)
    {
        rawState = BOTH_HEATERS_ON;
    }
    else 
    {
        rawState = HEATERS_OFF;
    }
    
    // Apply hysteresis to prevent rapid state changes
    if (rawState != lastState) 
    {
        // Only change state if we're clearly in a different range
        switch (lastState) 
        {
            case ONE_HEATER_ON:
                // Need to be clearly above 3.0A to switch to BOTH_HEATERS_ON
                if (current > 3.0) {
                    lastState = BOTH_HEATERS_ON;
                }
                // Need to be clearly below 1.3A to switch to BOTH_HEATERS_BLOWN  
                else if (current < 1.3) {
                    lastState = BOTH_HEATERS_BLOWN;
                }
                break;
                
            case BOTH_HEATERS_ON:
                // Need to be clearly below 2.6A to switch to ONE_HEATER_ON
                if (current < 2.6) {
                    lastState = ONE_HEATER_ON;
                }
                break;
                
            case BOTH_HEATERS_BLOWN:
                // Need to be clearly above 1.7A to switch to ONE_HEATER_ON
                if (current > 1.7) {
                    lastState = ONE_HEATER_ON;
                }
                break;
                
            default:
                lastState = rawState;
                break;
        }
    }
    
    #if DEBUG_SERIAL
        Serial.print("Current: ");
        Serial.print(current, 2);
        Serial.print("A, Raw state: ");
        Serial.print(heaterStateToString(rawState));
        Serial.print(", Final state: ");
        Serial.println(heaterStateToString(lastState));
    #endif
    
    return lastState;
}

//Add this function for periodic updates
void updateIrmsToFirebase() {
    unsigned long now = millis();
    
    // Update every 30 seconds
    //if (fbInitialized && (now - lastIrmsUpdate > 30000)) {
        if (Firebase.RTDB.setFloat(&fbData, "ESP32/control/IrmsReading", lastIrmsReading)) {
            #if DEBUG_SERIAL
                Serial.println("📊 Irms reading sent to Firebase");
            #endif
        //}
        lastIrmsUpdate = now;
    }
}
