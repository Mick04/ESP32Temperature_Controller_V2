// Track previous connection states for status publishing
#include "SystemStatusPublisher.h"
static int prevWifi = -1;
static int prevMqtt = -1;
static int prevFirebase = -1;
#include <Arduino.h>
#include "Config.h"
#include "WiFiManagerCustom.h"
#include "StatusLEDs.h"
#include "FirebaseService.h"
#include "TemperatureSensors.h"
#include "TimeManager.h"
#include "HeaterControl.h"
#include "MQTTManager.h"
#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Most ESP32 boards use GPIO2 for the onboard LED
#endif

// Use global variables from Globals.h
#include "Globals.h"

SystemStatus status; // Declare status variable
void turnOffLed(int index);

void setup()
{

  Serial.begin(115200);
  delay(1000); // Wait a moment before proceeding

  pinMode(LED_BUILTIN, OUTPUT);  // Initialize the BUILTIN_LED pin as an output
  pinMode(RELAY_PIN, OUTPUT);    // Initialize the RELAY_PIN as an output
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF (HIGH = OFF for active-low relay)
  delay(1000);                   // Wait a moment to ensure LEDs are ready

  initStatusLEDs(); // Initialize Status LEDs

  // status.heater = HEATERS_OFF; // Start with heater off (updated to new enum)
  turnOffLed(LED_WIFI);     // Turn off WiFi LED (index 0)
  turnOffLed(LED_FIREBASE); // Turn off Firebase LED (index 1)
  turnOffLed(LED_MQTT);     // Turn off MQTT LED (index 2)
  turnOffLed(LED_HEATER);   // Turn off Heater LED (index 3)

  delay(1000); // Wait a moment to ensure LEDs are ready

  status.wifi = CONNECTING; // Initial WiFi status
  initWiFi(status);         // Initialize WiFi
  initTemperatureSensors(); // Initialize Temperature Sensors

  delay(1000); // Wait a moment to ensure sensors are ready

  timeClient.begin();
  getTime(); // Initialize Time Manager

  delay(1000); // Wait a moment to ensure Time Manager is ready
}
void loop()
{
  // Publish Firebase heartbeat every 30 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000)
  {
    publishFirebaseHeartbeat();
    lastHeartbeat = millis();
  }

  // Publish status if any connection state changes
  if (status.wifi != prevWifi || status.mqtt != prevMqtt || status.firebase != prevFirebase)
  {
    publishAllSystemStatus(status);
    prevWifi = status.wifi;
    prevMqtt = status.mqtt;
    prevFirebase = status.firebase;
  }
  // handleWiFi(status);
  digitalWrite(LED_BUILTIN, HIGH); // LED ON
  delay(500);                      // Wait 500ms
  //==============================================
  //.           Wi-Fi Status Check.            ===
  //                start.                     ===
  //==============================================
  handleWiFi(status);

  // Handle Firebase connection status (will initialize when WiFi is ready)
  static bool firebaseInitialized = false;
  static bool scheduleDataFetched = false;
  if (status.wifi == CONNECTED && !firebaseInitialized)
  {
    initFirebase(status);
    firebaseInitialized = true;
  }

  // Fetch schedule data once after Firebase is connected
  if (firebaseInitialized && status.firebase == FB_CONNECTED && !scheduleDataFetched)
  {
    fetchScheduleDataFromFirebase();
    scheduleDataFetched = true;
  }

  if (firebaseInitialized)
  {
    handleFirebase(status);
  }
  //===============================================
  //  Check WiFi status and print RSSI value.
  //  this if statement can be moved to WiFiManagerCustom.cpp
  //  and published to MQTT broker for react app to read
  //===============================================
  if (WiFi.status() == WL_CONNECTED)
  {
    long rssi = WiFi.RSSI();

    // RSSI change detection and Firebase push
    static long prevRSSI = 0;
    static bool firstRSSIReading = true;
    static unsigned long lastRSSIPush = 0;

    // Push RSSI if it changes by 5dBm or more, or every 60 seconds, or first reading
    if (firstRSSIReading ||
        abs(rssi - prevRSSI) >= 5 ||
        millis() - lastRSSIPush > 60000)
    {

      if (fbInitialized)
      { // Only push if Firebase is initialized
        pushRSSIToFirebase(rssi);
        prevRSSI = rssi;
        firstRSSIReading = false;
        lastRSSIPush = millis();
      }
    }

    digitalWrite(LED_BUILTIN, LOW); // LED OFF
    delay(500);                     // Wait 500ms
  }

  //==============================================
  //.           Wi-Fi Status Check.            ===
  //                end.                       ===
  //==============================================

  /*************************************
   * Get the temperature from sensors  *
   *     This will be moved to         *
   *     the HeaterControl.cpp.        *
   *     function later                *
   *     start                         *
   ************************************/
  // If MQTT is connected, check for temperature changes and publish when needed
  static unsigned long lastMQTTCheck = 0;
  if (status.mqtt == MQTT_STATE_CONNECTED && millis() - lastMQTTCheck > 5000) // Check every 5 seconds
  {
    // Read all temperature sensors first
    readAllSensors();

    // Check if any temperature values have changed
    if (checkTemperatureChanges())
    {
      // Publish sensor data (includes time and system data)
      publishSensorData();
    }

    lastMQTTCheck = millis();
  }

  /**************************************
   * Get the temperature from sensors  *
   *     This will be moved to         *
   *     the HeaterControl.cpp.        *
   *     function later                *
   *     end                           *
   ************************************/

  /************************************
   * MQTT Connection Management.        *
   *     start                          *
   *************************************/

  // Handle MQTT connection (will initialize when WiFi is ready)
  static bool mqttInitialized = false;
  if (status.wifi == CONNECTED && !mqttInitialized)
  {
    initMQTT();
    mqttInitialized = true;
  }
  if (mqttInitialized)
  {
    handleMQTT();                  // This calls mqttClient.loop() internally
    status.mqtt = getMQTTStatus(); // Update MQTT status
  }

  // Update heater control and LEDs using the unified status object
  updateHeaterControl(status);
  /*************************************
   *   MQTT Connection Management.     *
   *    end                        *
   *************************************/
}
// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}