#include "network.hpp"
#include "config.hpp"
#include "sensorManager.hpp"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char *ssid = "Casa";
const char *password = "12345678";
// const char *ssid = "Wokwi-GUEST";
// const char *password = "";

const char *mqtt_server = "192.168.1.11";
// const char *mqtt_server = "172.18.0.15";
const int mqtt_port = 1883;
float target = 62;

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

    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("Message: ");
    Serial.println(message);

    if (String(topic) == "01/air_humidity_control") {
        Serial.println("Processing air humidity control message...");
        target = message.toFloat();
        float value = readHumidity();
    }
}

void setupMQTT() {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    ensureMQTTConnection();
    mqttClient.subscribe("01/air_humidity_control");  // Inscreve-se no tópico
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
