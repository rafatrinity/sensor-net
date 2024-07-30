#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>

#pragma region Pin_definitions
#define ECHO_PIN 18
#define TRIG_PIN 19
#define DHTPIN 4
#define DHTTYPE DHT22
#pragma region multiplexer
#define S0 13
#define S1 12
#define S2 14
#define S3 27
#define SIG_pin 36 // VP
#pragma endregion multiplexer
#pragma endregion Pin_definitions

// WiFi credentials
const char *ssid = "Wokwi-GUEST";
const char *password = "";

// MQTT server details
const char *mqtt_server = "172.17.0.4";
const int mqtt_port = 1883;

// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHTPIN, DHTTYPE);

#pragma region main
void setup() {
    Serial.begin(115200);
    connectToWiFi();
    initializePins();
    dht.begin();
    mqttClient.setServer(mqtt_server, mqtt_port);
}

void loop() {
    ensureMQTTConnection();
    publishSensorData();
    delay(3000);
}
#pragma endregion main

void connectToWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("OK! IP=");
    Serial.println(WiFi.localIP());
}

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);
    pinMode(SIG_pin, INPUT);
}

void ensureMQTTConnection() {
    while (!mqttClient.connected()) {
        Serial.println("Reconnecting to MQTT...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("Reconnected");
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
    Serial.println("MQTT connected...");
}

void publishSensorData() {
    publishDistanceData();
    publishTemperatureData();
    publishHumidityData();
    publishPhData();
    publishSoilHumidityData();
}

void publishDistanceData() {
    float distance = readDistanceCM();
    float percentage = (402 - distance) * 0.25;
    publishMQTTMessage("01/water_level", percentage);
}

void publishTemperatureData() {
    float temperature = readTemperature();
    publishMQTTMessage("01/temperature", temperature);
}

void publishHumidityData() {
    float humidity = readHumidity();
    publishMQTTMessage("01/air_humidity", humidity);
}

void publishPhData() {
    float ph = readAnalogMultiplexer(0) * 0.003418803;
    publishMQTTMessage("01/ph", ph);
}

void publishSoilHumidityData() {
    float soilHumidity = readAnalogMultiplexer(1) * 0.024420024;
    publishMQTTMessage("01/soil_humidity", soilHumidity);
}

void publishMQTTMessage(const char* topic, float value) {
    char message[50];
    snprintf(message, sizeof(message), "%.2f", value);
    mqttClient.publish(topic, message, true);
}

float readDistanceCM() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    int duration = pulseIn(ECHO_PIN, HIGH);
    return duration * 0.0342 / 2;
}

float readTemperature() {
    float temperature = dht.readTemperature();
    if (isnan(temperature)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return -999.0;
    }
    return temperature;
}

float readHumidity() {
    float humidity = dht.readHumidity();
    if (isnan(humidity)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return -999.0;
    }
    return humidity;
}

float readAnalogMultiplexer(int pin) {
    digitalWrite(S0, bitRead(pin, 0));
    digitalWrite(S1, bitRead(pin, 1));
    digitalWrite(S2, bitRead(pin, 2));
    digitalWrite(S3, bitRead(pin, 3));
    float value = analogRead(SIG_pin);
    return value;
}
