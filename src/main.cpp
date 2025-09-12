#include <Arduino.h>
#include "Config.h"
#include "WiFiManagerCustom.h"
#include "StatusLEDs.h"
#include "TemperatureSensors.h"
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Most ESP32 boards use GPIO2 for the onboard LED
#endif

SystemStatus status; // Declare status variable

// put function declarations here:
int myFunction(int, int);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);              // Give time for Serial to initialize
  initStatusLEDs();        // Initialize Status LEDs
  initTemperatureSensors();// Initialize Temperature Sensors
  status.wifi = CONNECTING; // Initial WiFi status
  initWiFi(status);         // Initialize WiFi
  
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
}
// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}