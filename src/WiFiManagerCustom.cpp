// ==================================================
// File: src/WiFiManagerCustom.cpp
// ==================================================

#include "WiFiManagerCustom.h"
#include <WiFi.h>
#include "StatusLEDs.h"
#include "Config.h"

void initWiFi(SystemStatus &status)
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to WiFi...");
    status.wifi = CONNECTING;
    updateLEDs(status);
    delay(10000); // delay 10 seconds to allow connection attempt
}

void handleWiFi(SystemStatus &status)
{
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED)
    {
        if (status.wifi != CONNECTED)
        {
            Serial.println("WiFi connected");
            long rssi = WiFi.RSSI();
            Serial.print("Signal strength (RSSI): ");
            Serial.print(rssi);
            Serial.println(" dBm");
        }
        status.wifi = CONNECTED;
        updateLEDs(status);
    }
    else if (s == WL_IDLE_STATUS || s == WL_NO_SSID_AVAIL || s == WL_DISCONNECTED)
    {
        status.wifi = CONNECTING;
        static unsigned long lastAttempt = 0;
        if (millis() - lastAttempt > 5000)
        {
            lastAttempt = millis();
            Serial.println("Reconnecting WiFi...");
            WiFi.disconnect();
            status.wifi = DISCONNECTED;
            updateLEDs(status);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }
    else
    {
        status.wifi = DISCONNECTED;
        updateLEDs(status);
    }
}
