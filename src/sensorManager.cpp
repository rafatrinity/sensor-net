#include "sensorManager.hpp"
#include "network.hpp"
#include "config.hpp"
#include <vector> 
#include <numeric>

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
    std::vector<int> arr;  
    
    for(int i = 0; i < 100; i++) {
        int analogValue = 4095 - analogRead(34);  
        if (analogValue > 0) {
            arr.push_back(analogValue);  
        }
    }
    
    if (!arr.empty()) {
        float sum = std::accumulate(arr.begin(), arr.end(), 0);
        float average = sum / arr.size();
        return average/40.95;  
    } else {
        return 0.0;  
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
    if(humidity != -999)
        publishMQTTMessage("01/air_humidity", humidity);
}

void publishPhData() {
    float ph = readPh();
    publishMQTTMessage("01/ph", ph);
}

void publishSoilHumidityData() {
    float soilHumidity = readSoilHumidity();
    if(soilHumidity > 0)
        publishMQTTMessage("01/soil_humidity", soilHumidity);
}
