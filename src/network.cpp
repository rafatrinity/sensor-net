#include "network.hpp"
#include "config.hpp"
#include "sensorManager.hpp"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AppConfig appConfig;

const int mqtt_port = 1883;
TargetValues target = {
    .airHumidity = 64.0f, 
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

    Serial.println("Connecting to WiFi...");
    WiFi.begin(appConfig.wifi.ssid, appConfig.wifi.password);

    uint32_t attempt = 0;

    while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
        vTaskDelay(retryDelay / portTICK_PERIOD_MS);
        Serial.print(".");
        spinner();
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(WiFi.localIP());
        LCD.setCursor(0, 0);
        LCD.print("IP:");
        LCD.print(WiFi.localIP().toString().c_str());
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        LCD.clear();

    } else {
        Serial.println("\nFailed to connect to WiFi");
        LCD.setCursor(0, 0);
        LCD.print("Failed to connect");
        if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi, retrying...");
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarde antes de tentar novamente
}

    }
    vTaskDelete(NULL);
}

void ensureMQTTConnection() {
    const int loopDelay = 500;
    while (!mqttClient.connected()) {
        Serial.println("Reconnecting to MQTT...");
        if (mqttClient.connect("ESP32Client1")) {
            Serial.println("Reconnected");
            if (mqttClient.publish("devices", appConfig.mqtt.roomTopic)) {
                Serial.print("Sent devices message: ");
                Serial.println(appConfig.mqtt.roomTopic);
            } else { Serial.println("Failed to send devices message");}
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            vTaskDelay(loopDelay / portTICK_PERIOD_MS);
        }
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

        // Atualizando e mostrando cada valor recebido
        if (!doc["airHumidity"].isNull() && doc["airHumidity"].is<double>()) {
            float newAirHumidity = doc["airHumidity"].as<float>();
            target.airHumidity = newAirHumidity;
            Serial.print("Updated airHumidity to: ");
            Serial.println(target.airHumidity);
        } else {
            Serial.println("airHumidity not found or not a number");
        }

        if (!doc["vpd"].isNull() && doc["vpd"].is<double>()) {
            float newVpd = doc["vpd"].as<float>();
            target.vpd = newVpd;
            Serial.print("Updated vpd to: ");
            Serial.println(target.vpd);
        } else {
            Serial.println("vpd not found or not a number");
        }

        if (!doc["soilHumidity"].isNull() && doc["soilHumidity"].is<double>()) {
            float newSoilHumidity = doc["soilHumidity"].as<float>();
            target.soilHumidity = newSoilHumidity;
            Serial.print("Updated soilHumidity to: ");
            Serial.println(target.soilHumidity);
        } else {
            Serial.println("soilHumidity not found or not a number");
        }

        if (!doc["temperature"].isNull() && doc["temperature"].is<double>()) {
            float newTemperature = doc["temperature"].as<float>();
            target.temperature = newTemperature;
            Serial.print("Updated temperature to: ");
            Serial.println(target.temperature);
        } else {
            Serial.println("temperature not found or not a number");
        }
        Serial.println("===================================");
    }
}


void setupMQTT() {
    mqttClient.setServer(appConfig.mqtt.server, appConfig.mqtt.port);
    mqttClient.setCallback(mqttCallback);
    ensureMQTTConnection();
    mqttClient.subscribe((String(appConfig.mqtt.roomTopic) + "/control").c_str());
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
        LCD.clear();
        LCD.setCursor(0, 0);
        LCD.print(topic);
        LCD.setCursor(0, 1);
        LCD.print(message);
        lastMessage = message;
    }

    if (mqttClient.publish(topic, message.c_str(), false)) {
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(value);
    } else {
        Serial.println(mqttClient.state());
        Serial.println("Failed to publish message.");
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}
