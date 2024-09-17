#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    initializePins();
    initializeSensors();
    
    LCD.init();
    LCD.backlight();
    Serial.println(esp_get_free_heap_size());
    xTaskCreatePinnedToCore(
        connectToWiFi, 
        "WiFiTask", 
        4096, 
        NULL, 
        1, 
        NULL, 
        0 
    );

    // xTaskCreatePinnedToCore(
    //     manageMQTT, 
    //     "MQTTTask", 
    //     2048, 
    //     NULL, 
    //     1, 
    //     NULL, 
    //     0 
    // );

    xTaskCreatePinnedToCore(
        readSensors, 
        "SensorTask", 
        2048, 
        NULL, 
        1, 
        NULL, 
        1 
    );
}

void loop() {}

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
}

void readSensors(void * parameter) {
    const int loopDelay = 500;
    while(true) {
        publishDistanceData();
        publishTemperatureData();
        publishHumidityData();
        publishPhData();
        publishSoilHumidityData();
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}
