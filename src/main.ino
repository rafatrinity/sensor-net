#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    initializeSensors();
    pinMode(2, OUTPUT);
    LCD.init();
    LCD.backlight();
    Serial.println(esp_get_free_heap_size());
    xTaskCreate(
        connectToWiFi, 
        "WiFiTask", 
        4096, 
        NULL, 
        1, 
        NULL
    );

    xTaskCreate(
        manageMQTT, 
        "MQTTTask", 
        2048, 
        NULL, 
        1, 
        NULL
    );

    xTaskCreate(
        readSensors, 
        "SensorTask", 
        2048, 
        NULL, 
        1, 
        NULL 
    );
}

void readSensors(void * parameter) {
    const int loopDelay = 500;
    while(true) {
        publishTemperatureData();
        publishHumidityData();
        publishSoilHumidityData();
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}

void loop() {}