#include "sensorManager.hpp"
#include "network.hpp"
#include "config.hpp"

DHT dht(DHTPIN, DHTTYPE);

void initializeSensors() {
    dht.begin();
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

float readPh() {
    return analogRead(35) * 0.003418803;
}

float readSoilHumidity() {
    // return analogRead(34) * 0.024420024;
    return analogRead(34);
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
    float ph = readPh();
    publishMQTTMessage("01/ph", ph);
}

void publishSoilHumidityData() {
    float soilHumidity = readSoilHumidity();
    publishMQTTMessage("01/soil_humidity", soilHumidity);
}
