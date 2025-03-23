#include "network.hpp"
#include "config.hpp"
#include "sensorManager.hpp"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AppConfig appConfig;

const int mqtt_port = 1883;
TargetValues target = {
    .airHumidity = 70.0f,
    .vpd = 1.0f,
    .soilHumidity = 66.0f,
    .temperature = 25.0f
};

void spinner() {
    static int8_t counter = 0;
    static int8_t lastCounter = -1;
    const char* glyphs = "\xa1\xa5\xdb";

    if (counter != lastCounter) {
        LCD.setCursor(15, 1);
        LCD.print(glyphs[counter]);
        lastCounter = counter;
    }
    counter = (counter + 1) % strlen(glyphs);
}

void connectToWiFi(void * parameter) {
    const uint32_t maxRetries = 20;
    const uint32_t retryDelay = 500;

    while (true) {
        Serial.println("Connecting to WiFi...");
        WiFi.begin(appConfig.wifi.ssid, appConfig.wifi.password);

        uint32_t attempt = 0;
        while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
            vTaskDelay(retryDelay / portTICK_PERIOD_MS);
            Serial.print(".");
            xSemaphoreTake(lcdMutex, portMAX_DELAY);
            spinner();
            xSemaphoreGive(lcdMutex);
            attempt++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(WiFi.localIP());
            xSemaphoreTake(lcdMutex, portMAX_DELAY);
            LCD.setCursor(0, 0);
            LCD.print("IP:");
            LCD.print(WiFi.localIP().toString().c_str());
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            LCD.clear();
            xSemaphoreGive(lcdMutex);
            break;
        } else {
            Serial.println("\nFailed to connect to WiFi, retrying in 5 seconds...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

void ensureMQTTConnection() {
    const int loopDelay = 500;
    while (!mqttClient.connected()) {
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        Serial.println("Reconnecting to MQTT...");
        if (mqttClient.connect("ESP32Client1")) {
            Serial.println("Reconnected");
            if (mqttClient.publish("devices", appConfig.mqtt.roomTopic)) {
                Serial.print("Sent devices message: ");
                Serial.println(appConfig.mqtt.roomTopic);
            } else {
                Serial.println("Failed to send devices message");
            }
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            vTaskDelay(loopDelay / portTICK_PERIOD_MS);
        }
        xSemaphoreGive(mqttMutex);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("===================================");
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("Raw Message: ");
    Serial.println(message);

    if (String(topic) == String(appConfig.mqtt.roomTopic) + "/control") {
        Serial.println("Processing control message...");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Serial.print("JSON deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }

        // Atualizando valores existentes
        if (!doc["airHumidity"].isNull() && doc["airHumidity"].is<double>()) {
            target.airHumidity = doc["airHumidity"].as<float>();
            Serial.print("Updated airHumidity to: ");
            Serial.println(target.airHumidity);
        }
        
        if (!doc["vpd"].isNull() && doc["vpd"].is<double>()) {
            target.vpd = doc["vpd"].as<float>();
            Serial.print("Updated vpd to: ");
            Serial.println(target.vpd);
        }

        if (!doc["soilHumidity"].isNull() && doc["soilHumidity"].is<double>()) {
            target.soilHumidity = doc["soilHumidity"].as<float>();
            Serial.print("Updated soilHumidity to: ");
            Serial.println(target.soilHumidity);
        }

        if (!doc["temperature"].isNull() && doc["temperature"].is<double>()) {
            target.temperature = doc["temperature"].as<float>();
            Serial.print("Updated temperature to: ");
            Serial.println(target.temperature);
        }

        if (doc["lightOnTime"].is<String>()) {
            sscanf(doc["lightOnTime"], "%d:%d", &target.lightOnTime.tm_hour, &target.lightOnTime.tm_min);
            Serial.printf("Horário de ligar atualizado para %02d:%02d\n", target.lightOnTime.tm_hour, target.lightOnTime.tm_min);
        }
    
        if (doc["lightOffTime"].is<String>()) {
            sscanf(doc["lightOffTime"], "%d:%d", &target.lightOffTime.tm_hour, &target.lightOffTime.tm_min);
            Serial.printf("Horário de desligar atualizado para %02d:%02d\n", target.lightOffTime.tm_hour, target.lightOffTime.tm_min);
        }
    }
}

void setupMQTT() {
    mqttClient.setServer(appConfig.mqtt.server, appConfig.mqtt.port);
    mqttClient.setCallback(mqttCallback);
    ensureMQTTConnection();
    xSemaphoreTake(mqttMutex, portMAX_DELAY);
    if(mqttClient.subscribe((String(appConfig.mqtt.roomTopic) + "/control").c_str())){
        Serial.println("Subscribed to control topic");
    } else {
        Serial.println("Failed to subscribe to control topic");
    }
    xSemaphoreGive(mqttMutex);
}

void manageMQTT(void * parameter) {
    setupMQTT();
    const int loopDelay = 500;
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            ensureMQTTConnection();
            mqttClient.loop();
        } else {
            Serial.println("WiFi disconnected, waiting to reconnect...");
        }
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}

void publishMQTTMessage(const char* topic, float value) {
    static String lastMessage = "";
    String message = String(value, 2);

    if (lastMessage != message) {
        xSemaphoreTake(lcdMutex, portMAX_DELAY);
        LCD.setCursor(0, 0);
        LCD.print(topic);
        LCD.setCursor(0, 1);
        LCD.print(message);
        xSemaphoreGive(lcdMutex);
        lastMessage = message;
    }

    xSemaphoreTake(mqttMutex, portMAX_DELAY);
    if (mqttClient.publish(topic, message.c_str(), false)) {
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(value);
    } else {
        Serial.println(mqttClient.state());
        Serial.println("Failed to publish message.");
    }
    xSemaphoreGive(mqttMutex);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}