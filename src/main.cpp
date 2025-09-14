#include <Arduino.h>
#include "Config.h"
#include "WiFiManagerCustom.h"
#include "StatusLEDs.h"
#include "TemperatureSensors.h"
#include "TimeManager.h"
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Most ESP32 boards use GPIO2 for the onboard LED
#endif
#include "TempInput.h"

// Declare global objects for timeClient
// WiFiUDP ntpUDP; // Commented out to avoid multiple definitions
// NTPClient timeClient(ntpUDP); // Commented out to avoid multiple definitions

SystemStatus status; // Declare status variable

// put function declarations here:
int myFunction(int, int);

void setup()
{

  Serial.begin(115200);
  delay(1000); // Give time for Serial to initialize
  Serial.println("\n=== ESP32 Temperature Controller Starting ===");
  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("Free PSRAM: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");
  delay(1000); // Wait a moment before proceeding

  pinMode(LED_BUILTIN, OUTPUT); // Initialize the BUILTIN_LED pin as an output

  Serial.println("✅ Basic hardware initialized");
  delay(1000); // Wait a moment to ensure LEDs are ready

  initStatusLEDs(); // Initialize Status LEDs

  Serial.println("✅ Status LEDs initialized");
  delay(1000); // Wait a moment to ensure LEDs are ready

  status.wifi = CONNECTING; // Initial WiFi status

  Serial.println("✅ WiFi initialization started");

  initWiFi(status); // Initialize WiFi

  initTemperatureSensors(); // Initialize Temperature Sensors

  Serial.println("✅ Temperature sensors initialized");
  delay(1000); // Wait a moment to ensure sensors are ready

  delay(1000); // Wait a moment to ensure LEDs are ready

  timeClient.begin();
  getTime(); // Initialize Time Manager

  Serial.println("✅ Time Manager initialized");
  Serial.print("Hours ");
  Serial.print(Hours);
  Serial.print(": Minutes ");
  Serial.println(Minutes);
  Serial.print("currentDay ");
  Serial.print(currentDay);
  Serial.print(": currentMonth ");
  Serial.println(currentMonth);
  Serial.println("✅ Time Manager initialized");
  Serial.println("✅ Time Manager initialized");
  Serial.println("✅ Time Manager initialized");
  delay(1000); // Wait a moment to ensure Time Manager is ready
  // status.heater = HEATER_OFF; // Start with heater off
}
void loop()
{
  // handleWiFi(status);
  digitalWrite(LED_BUILTIN, HIGH); // LED ON
  Serial.println("LED ON");
  delay(500); // Wait 500ms
  //==============================================
  //.           Wi-Fi Status Check.            ===
  //                start.                     ===
  //==============================================
  handleWiFi(status);
  //===============================================
  //  Check WiFi status and print RSSI value.
  //  this if statement can be moved to WiFiManagerCustom.cpp
  //  and published to MQTT broker for react app to read
  //===============================================
  if (WiFi.status() == WL_CONNECTED)
  {
    long rssi = WiFi.RSSI();
    Serial.println("===============================");
    Serial.print("WiFi RSSI: ");
    Serial.println(rssi);
    Serial.println("**********************");
    Serial.println(" ");
    digitalWrite(LED_BUILTIN, LOW); // LED OFF
    Serial.println("LED OFF");
    delay(500); // Wait 500ms

//Serial.prints can be deleted when programming is complete
    Serial.println("===============================");
    if (rssi > -50)
    {
      Serial.println("🎉🎉🎉Excellent signal strength");
    }
    else if (rssi > -60)
    {
      Serial.println("🌞 Very good signal strength");
    }
    else if (rssi > -70)
    {
      Serial.println("🌼 Good signal strength");
    }
    else if (rssi > -80)
    {
      Serial.println("🌸 Low signal strength");
    }
    else
    {
      Serial.println("🌑 Very low signal strength");
    }
    Serial.println("==============================");
    Serial.println("   ");
  }
  else
  {
    Serial.println("WiFi is not connected.");
  }
  //==============================================
  //.           Wi-Fi Status Check.            ===
  //                end.                       ===
  //==============================================

  Serial.println("Looping...");
  Serial.print("WiFi Status: ");
  /*************************************
   * Get the temperature from sensors  *
   *     This will be moved to         *
   *     the HeaterControl.cpp.        *
   *     function later                *
   *     start                         *
   ************************************/
  readAllSensors();

  /**************************************
   * Get the temperature from sensors  *
   *     This will be moved to         *
   *     the HeaterControl.cpp.        *
   *     function later                *
   *     end                           *
   ************************************/
  /**/
  // switch (status.wifi)
  // {
  // // case CONNECTING:
  // //   Serial.println("CONNECTING");
  // //   updateLEDs(status);
  // //   delay(10000); // Wait 10 seconds to allow LED update to be visible
  // //   break;
  // // case CONNECTED:
  // //   Serial.println("CONNECTED");
  // //   updateLEDs(status);
  // //   break;
  // // case ERROR:
  // //   Serial.println("ERROR");
  // //   break;
  // // }
  // if (status.wifi == CONNECTED) {
  //       Serial.println("WiFi is connected.");
  //   } else {
  //       Serial.println("WiFi is not connected.");
  //   }

  // inputAmTempFromSerial();
  // inputPmTempFromSerial();
  // inputAmTimeFromSerial();
  // inputPmTimeFromSerial();
}
// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}