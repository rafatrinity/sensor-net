#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    initializePins();
    initializeSensors();

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
}

void loop() {}

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
}

void readSensors(void * parameter) {
    const int loopDelay = 100;
    while(true) {
        publishDistanceData();
        publishTemperatureData();
        publishHumidityData();
        publishPhData();
        publishSoilHumidityData();
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}
