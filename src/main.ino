#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    initializeSensors();
    pinMode(2, OUTPUT);

    Wire.begin(21, 22);
    LCD.init();
    LCD.backlight();

    xTaskCreatePinnedToCore(
        connectToWiFi, 
        "WiFiTask", 
        4096, 
        NULL, 
        1, 
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        manageMQTT, 
        "MQTTTask", 
        4096, 
        NULL, 
        1, 
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        readSensors, 
        "SensorTask", 
        4096, 
        NULL, 
        1, 
        NULL,
        1
    );
}

void readSensors(void * parameter) {
    const int loopDelay = 2000;
    while (true) {
        float temperature = readTemperature();
        float airHumidity = readHumidity();
        float soilHumidity = readSoilHumidity();
        float vpd = calculateVpd(temperature, airHumidity);
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);

        if(isnan(vpd)) continue;
        
        String payload = "{";
        payload += "\"temperature\":" + String(temperature, 2) + ",";
        payload += "\"airHumidity\":" + String(airHumidity, 2) + ",";
        payload += "\"soilHumidity\":" + String(soilHumidity, 2) + ",";
        payload += "\"vpd\":" + String(vpd, 3);
        payload += "}";

        if (mqttClient.publish(ROOM "/sensors", payload.c_str(), true)) {
            Serial.println("Sensor data published: " + payload);
        } else {
            Serial.println("Failed to publish sensor data.");
        }

        LCD.clear();
        LCD.setCursor(0, 0);
        LCD.print("Temp:");
        LCD.print(temperature, 1);
        LCD.print("C");

        LCD.setCursor(0, 1);
        LCD.print("A:");
        LCD.print(airHumidity, 1);
        LCD.print(" S:");
        LCD.print(soilHumidity, 1);
    }
}

void loop() {}
