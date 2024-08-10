#include "network.hpp"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqtt_server = "172.17.0.2";
const int mqtt_port = 1883;

void connectToWiFi(void * parameter) {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("OK! IP=");
    Serial.println(WiFi.localIP());

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
