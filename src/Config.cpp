// Config.cpp

#include "Config.h"

const char* heaterStateToString(HeaterState state) {
    switch (state) {
        case HEATER_STARTUP:     return "HEATER_STARTUP";
        case HEATERS_OFF:        return "HEATERS_OFF";
        case ONE_HEATER_ON:      return "ONE_HEATER_ON";
        case BOTH_HEATERS_ON:    return "BOTH_HEATERS_ON";
        case BOTH_HEATERS_BLOWN: return "BOTH_HEATERS_BLOWN";
        default:                 return "UNKNOWN_STATE";
    }
}