// ==================================================
// File: src/TemperatureSensors.cpp
// ==================================================

#include "TemperatureSensors.h"

// Temperature sensor setup
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// Sensor addresses
DeviceAddress red, blue, green;
static int connectedSensors = 0;

void initTemperatureSensors()
{
    sensors.begin();
    connectedSensors = sensors.getDeviceCount();

    // Get the addresses of the sensors
    sensors.getAddress(red, 0);
    sensors.getAddress(blue, 1);
    sensors.getAddress(green, 2);
}

/**************************************
 * Get the temperature from a sensor  *
 *           start                    *
 *************************************/
float getTemperature(int sensorIndex)
{
    DeviceAddress *sensorAddress;

    // Select the appropriate sensor address
    switch (sensorIndex)
    {
    case 0:
        sensorAddress = &red;
        break;
    case 1:
        sensorAddress = &blue;
        break;
    case 2:
        sensorAddress = &green;
        break;
    default:
        return NAN; // Invalid sensor index
    }

    // Request temperature from specific sensor
    sensors.requestTemperatures();
    float temp = sensors.getTempC(*sensorAddress);

    // Check if reading is valid
    if (temp == DEVICE_DISCONNECTED_C)
    {
        return NAN;
    }

    return temp;
}
/**************************************
 * Get the temperature from a sensor  *
 *           end                      *
 *************************************/

void readAllSensors()
{
    // Request temperatures from all sensors at once (more efficient)
    sensors.requestTemperatures();
}
