// ==================================================
// File: src/StatusLEDs.cpp
// ==================================================

#include "StatusLEDs.h"
#include "Config.h"
#include "MQTTManager.h" // For MQTT status publishing

extern PubSubClient mqttClient;
// Define the LED array
CRGB leds[NUM_LEDS];

void initStatusLEDs()
{
    FastLED.addLeds<LED_TYPE, WS2811_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50); // Set brightness to 50/255
    FastLED.clear();
    leds[LED_HEATER] = CRGB::Black; // Ensure heater LED is off at startup
    FastLED.show();                 // Initial clear
}

void updateLEDs(SystemStatus &status)
{
    // Clear all LEDs first
    // FastLED.clear();

    // Set WiFi LED based on status
    switch (status.wifi)
    {
    case CONNECTING:
        leds[LED_WIFI] = CRGB::Blue;
        break;
    case CONNECTED:
        leds[LED_WIFI] = CRGB::Green;
        break;
    case DISCONNECTED:
        leds[LED_WIFI] = CRGB::Red;
        break;
    }

    // Set Firebase LED based on status
    switch (status.firebase)
    {
    case FB_CONNECTING:
        leds[LED_FIREBASE] = CRGB::Blue;
        break;
    case FB_CONNECTED:
        leds[LED_FIREBASE] = CRGB::Green;
        break;
    case FB_ERROR:
        leds[LED_FIREBASE] = CRGB::Red;
        break;
    }

    // Set MQTT LED based on status
    switch (status.mqtt)
    {
    case MQTT_STATE_DISCONNECTED:
        leds[LED_MQTT] = CRGB::Black; // Off
        break;
    case MQTT_STATE_CONNECTING:
        leds[LED_MQTT] = CRGB::Blue;
        break;
    case MQTT_STATE_CONNECTED:
        leds[LED_MQTT] = CRGB::Green;
        break;
    case MQTT_STATE_ERROR:
        leds[LED_MQTT] = CRGB::Red;
        break;
    }

    // Set Heater LED based on status
    switch (status.heater)
    {
    case HEATERS_OFF:
        leds[LED_HEATER] = CRGB::Green; // No heaters running
        break;
    case ONE_HEATER_ON:
        leds[LED_HEATER] = CRGB::Orange; // One heater running
        break;
    case BOTH_HEATERS_ON:
        leds[LED_HEATER] = CRGB::Blue; // Both heaters running
        break;
    case BOTH_HEATERS_BLOWN:
        leds[LED_HEATER] = CRGB::Red; // Both heaters blown
        break;
    }

    FastLED.show();
}

// Helper to turn on just one LED at a time
void showSingleLed(int index, CRGB color)
{
    // FastLED.clear();
    leds[index] = color;
    FastLED.show();
}
void turnOffLed(int index)
{
    leds[index] = CRGB::Black;
    FastLED.show();
}
