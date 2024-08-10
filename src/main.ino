#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>

#define ECHO_PIN 25
#define TRIG_PIN 26
#define DHTPIN 4
#define DHTTYPE DHT22

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const char *mqtt_server = "172.17.0.2";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHTPIN, DHTTYPE);

void connectToWiFi(void * parameter);
void manageMQTT(void * parameter);
void readSensors(void * parameter);

void setup() {
    Serial.begin(115200);
    initializePins();
    dht.begin();

    xTaskCreatePinnedToCore(
        connectToWiFi, /* função da tarefa */
        "WiFiTask", /* nome da tarefa */
        4096, /* tamanho da pilha */
        NULL, /* parâmetros da tarefa */
        1, /* prioridade da tarefa */
        NULL, /* handle da tarefa */
        0 /* núcleo em que a tarefa será executada (0 ou 1) */
    );

    xTaskCreatePinnedToCore(
        manageMQTT, /* função da tarefa */
        "MQTTTask", /* nome da tarefa */
        4096, /* tamanho da pilha */
        NULL, /* parâmetros da tarefa */
        1, /* prioridade da tarefa */
        NULL, /* handle da tarefa */
        0 /* núcleo em que a tarefa será executada (0 ou 1) */
    );

    xTaskCreatePinnedToCore(
        readSensors, /* função da tarefa */
        "SensorTask", /* nome da tarefa */
        4096, /* tamanho da pilha */
        NULL, /* parâmetros da tarefa */
        1, /* prioridade da tarefa */
        NULL, /* handle da tarefa */
        1 /* núcleo em que a tarefa será executada (0 ou 1) */
    );
}

void loop() {
}

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

void initializePins() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
}

void publishMQTTMessage(const char* topic, float value) {
    char message[50];
    snprintf(message, sizeof(message), "%.2f", value);
    mqttClient.publish(topic, message, true);
}

void readSensors(void * parameter) {
    while(true) {
        publishDistanceData();
        publishTemperatureData();
        publishHumidityData();
        publishPhData();
        publishSoilHumidityData();
    }
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
    float ph = analogRead(35) * 0.003418803;
    publishMQTTMessage("01/ph", ph);
}

void publishSoilHumidityData() {
    float soilHumidity = analogRead(34) * 0.024420024;
    publishMQTTMessage("01/soil_humidity", soilHumidity);
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
