#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <Wire.h> // Biblioteca para comunicação I2C
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);

void readSensors(void * parameter);

void setup() {
    Serial.begin(BAUD);
    initializeSensors();
    pinMode(2, OUTPUT);

    // Configurar SDA e SCL
    Wire.begin(21, 22); // Define SDA = GPIO 21 e SCL = GPIO 22

    // Inicializar o display LCD
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
    const int loopDelay = 2000; // Atualizar a cada 2 segundos
    while (true) {
        float temperature = readTemperature();
        float airHumidity = readHumidity();
        float soilHumidity = readSoilHumidity();

        // Montar payload para MQTT
        String payload = "{";
        payload += "\"temperature\":" + String(temperature, 2) + ",";
        payload += "\"humidity\":" + String(airHumidity, 2) + ",";
        payload += "\"soil_humidity\":" + String(soilHumidity, 2);
        payload += "}";

        if (mqttClient.publish("01/sensors", payload.c_str(), true)) {
            Serial.println("Sensor data published: " + payload);
        } else {
            Serial.println("Failed to publish sensor data.");
        }

        // Atualizar display LCD
        LCD.clear();
        LCD.setCursor(0, 0); // Primeira linha
        LCD.print("Temp:");
        LCD.print(temperature, 1);
        LCD.print("C");

        LCD.setCursor(0, 1); // Segunda linha
        LCD.print("Air:");
        LCD.print(airHumidity, 1);
        LCD.print("Soil:");
        LCD.print(soilHumidity, 1);

        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}

void loop() {}
