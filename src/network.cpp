#include "network.hpp"
#include "config.hpp"
#include "sensorManager.hpp"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = "Casa";
const char *password = "12345678";
// const char *ssid = "Wokwi-GUEST";
// const char *password = "";

const char *mqtt_server = "192.168.1.11";
// const char *mqtt_server = "172.18.0.15";
const int mqtt_port = 1883;
TargetValues target;

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
    const uint32_t retryDelay = 100;

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

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
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            vTaskDelay(loopDelay / portTICK_PERIOD_MS);
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);

    // Converta o payload em uma string
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("Message: ");
    Serial.println(message);

    // Verifique se a mensagem chegou no tópico esperado
    if (String(topic) == ROOM "/control") {
        Serial.println("Processing control message...");

        // Crie um buffer estático para evitar realocações dinâmicas
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            Serial.print("JSON deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }

        // Atualize os valores do alvo (target)
        target.airHumidity = doc["airHumidity"] | 0.0f;
        target.vpd = doc["vpd"] | 0.0f;
        target.soilHumidity = doc["soilHumidity"] | 0.0f;
        target.temperature = doc["temperature"] | 0.0f;

        Serial.println("Updated target values:");
        Serial.print("Air Humidity: ");
        Serial.println(target.airHumidity);
        Serial.print("VPD: ");
        Serial.println(target.vpd);
        Serial.print("Soil Humidity: ");
        Serial.println(target.soilHumidity);
        Serial.print("Temperature: ");
        Serial.println(target.temperature);
    }
}

void setupMQTT() {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    ensureMQTTConnection();
    mqttClient.subscribe(ROOM "/control");
}

void manageMQTT(void * parameter) {
    setupMQTT();
    const int loopDelay = 100;
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

    if (mqttClient.publish(topic, message.c_str(), true)) {
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(value);
    } else {
        Serial.println(mqttClient.state());
        Serial.println("Failed to publish message.");
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}
