#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    // initializePins();
    initializeSensors();
    pinMode(2, OUTPUT);
    LCD.init();
    LCD.backlight();
    xTaskCreatePinnedToCore(
        connectToWiFi, 
        "WiFiTask", 
        8192, 
        NULL, 
        1, 
        NULL, 
        0 
    );

    xTaskCreatePinnedToCore(
        manageMQTT, 
        "MQTTTask", 
        8192, 
        NULL, 
        1, 
        NULL, 
        0 
    );

    xTaskCreatePinnedToCore(
        readSensors, 
        "SensorTask", 
        8192, 
        NULL, 
        1, 
        NULL, 
        1 
    );
    Serial.println(ESP.getFreeHeap());
    Serial.println(esp_get_free_heap_size());
}

void loop() {}

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
}

void readSensors(void * parameter) {
    const int loopDelay = 500;
    while(true) {
        // publishDistanceData();
        publishTemperatureData();
        publishHumidityData();
        // publishPhData();
        publishSoilHumidityData();
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}
