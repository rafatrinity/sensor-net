#include "network.hpp"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqtt_server = "172.17.0.3";
const int mqtt_port = 1883;

void connectToWiFi(void * parameter) {
    const uint32_t maxRetries = 20;
    const uint32_t retryDelay = 100;

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    uint32_t attempt = 0;

    while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
        vTaskDelay(retryDelay / portTICK_PERIOD_MS);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi");
        ESP.restart();
    }
    vTaskDelete(NULL);
}

void ensureMQTTConnection() {
    while (!mqttClient.connected()) {
        Serial.println("Reconnecting to MQTT...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("Reconnected");
            mqttClient.subscribe("your/topic");
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

void manageMQTT(void * parameter) {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
        Serial.print("Message arrived [");
        Serial.print(topic);
        Serial.print("] ");
        for (int i = 0; i < length; i++) {
            Serial.print((char)payload[i]);
        }
        Serial.println();
    });

    while(true) {
        ensureMQTTConnection();
        mqttClient.loop();
    }
}

void publishMQTTMessage(const char* topic, float value) {
    char message[50];
    snprintf(message, sizeof(message), "%.2f", value);
    mqttClient.publish(topic, message, true);
}
