#include "config.hpp"
#include "network.hpp"
#include "sensorManager.hpp"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

LiquidCrystal_I2C LCD(0x27, 16, 2);
JsonDocument doc;

void readSensors(void * parameter);

void lightControlTask(void *parameter) {
    while (true) {
        lightControl(target.lightOnTime, target.lightOffTime, appConfig.gpioControl.timeControlTestPin);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(BAUD);
    initializeSensors();
    pinMode(appConfig.gpioControl.humidityControlPin, OUTPUT);
    pinMode(appConfig.gpioControl.timeControlTestPin, OUTPUT);
    Wire.begin(21, 22);
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
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    initializeNTP();

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

    xTaskCreatePinnedToCore(
        lightControlTask,
        "LightControlTask",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    Serial.println(ESP.getFreeHeap());
    Serial.println(esp_get_free_heap_size());
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
        
        ensureMQTTConnection();
        
        doc["temperature"] = temperature;
        doc["airHumidity"] = airHumidity;
        doc["soilHumidity"] = soilHumidity;
        doc["vpd"] = vpd;

        String payload;
        serializeJson(doc, payload);

        if (!mqttClient.publish((String(appConfig.mqtt.roomTopic) + "/sensors").c_str(), payload.c_str(), true)) {
            Serial.println("Failed to publish sensor data.");
        }

        static float lastTemperature = -1;
        static float lastAirHumidity = -1;
        static float lastSoilHumidity = -1;

        if (temperature != lastTemperature) {
            LCD.setCursor(0, 0);
            LCD.print("Temp:");
            LCD.print(temperature, 1);
            LCD.print("C");
            lastTemperature = temperature;
        }

        if (airHumidity != lastAirHumidity || soilHumidity != lastSoilHumidity) {
            LCD.setCursor(0, 1);
            LCD.print("A:");
            LCD.print(airHumidity, 1);
            LCD.print(" S:");
            LCD.print(soilHumidity, 1);
            lastAirHumidity = airHumidity;
            lastSoilHumidity = soilHumidity;
        }
    }
}

void loop() {}
