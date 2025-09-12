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
    Serial.println("Initializing temperature sensors...");
    sensors.begin();

    connectedSensors = sensors.getDeviceCount();
    Serial.print("Found ");
    Serial.print(connectedSensors);
    Serial.println(" temperature sensors");

    /**************************************
     * Get the addresses of the sensors  *
     *           start                   *
     *************************************/
    if (!sensors.getAddress(red, 0))
    {
        Serial.println("red sensor not found.");
    }
    else
    {
        Serial.println("red sensor detected.");
        delay(100);
    }

    if (!sensors.getAddress(blue, 1))
    {
        Serial.println("blue sensor not found.");
    }
    else
    {
        Serial.println("blue sensor detected.");
        delay(100);
    }

    if (!sensors.getAddress(green, 2))
    {
        Serial.println("green sensor not found.");
    }
    else
    {
        Serial.println("green sensor detected.");
        delay(100);
    }
    /**************************************
     * Get the addresses of the sensors  *
     *           end                     *
     *************************************/
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
        Serial.print("Sensor ");
        Serial.print(sensorIndex);
        Serial.println(" disconnected");
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
    // remove when done testing
    /**************************/
    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println("***=============================***");

    Serial.println("Reading all temperature sensors:");
    /********************************/
    for (int i = 0; i < 3; i++)
    {
        float temp = getTemperature(i);
        // remove when done testing
        /***************************/
        if (!isnan(temp))
        {
            if (i == 0)
                Serial.print("Red ");
            if (i == 1)
                Serial.print("Blue ");
            if (i == 2)
                Serial.print("Green ");
            // Serial.print("Sensor ");
            // Serial.print(i);
            // Serial.print(": ");
            Serial.print(temp);
            Serial.println("°C");
        }
    }
    Serial.println("");
    Serial.println("***=============================***");
    Serial.println("");
    Serial.println("");
    Serial.println("");
    // remove when done testing
    /***************************/
}

// int getConnectedSensorCount()
// {
//     return connectedSensors;
// }
