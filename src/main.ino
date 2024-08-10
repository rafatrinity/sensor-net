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

void loop() {
}

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
}

void readSensors(void * parameter) {
    while(true) {
        publishDistanceData();
        publishTemperatureData();
        publishHumidityData();
        publishPhData();
        publishSoilHumidityData();
    }
}
