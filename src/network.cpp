#include "network.hpp"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqtt_server = "172.17.0.4";
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
    const int loopDelay = 500;
    while (!mqttClient.connected()) {
        Serial.println("Reconnecting to MQTT...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("Reconnected");
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            vTaskDelay(loopDelay / portTICK_PERIOD_MS);
        }
    }
}

void manageMQTT(void * parameter) {
    const int loopDelay = 100;
    mqttClient.setServer(mqtt_server, mqtt_port);
    while(true) {
        ensureMQTTConnection();
        mqttClient.loop();
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}

void publishMQTTMessage(const char* topic, float value) {
    String message = String(value, 2);
    if (mqttClient.publish(topic, message.c_str(), true)) {
        Serial.println("Message published successfully.");
    } else {
        Serial.println("Failed to publish message.");
    }
}