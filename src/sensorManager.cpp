#include <algorithm>
#include <numeric>
#include <numeric>
#include <vector>

#include "sensorManager.hpp"
#include "network.hpp"
#include "config.hpp"

DHT dht(DHTPIN, DHTTYPE);

extern float target;

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
    
    if(!target) digitalWrite(2, LOW);

    if (humidity < target) {
        digitalWrite(2, LOW);
        Serial.println("Relay activated: Humidity is below target.");
    } 
    else if (humidity > target) {
        digitalWrite(2, HIGH);
        Serial.println("Relay deactivated: Humidity is above target.");
    }
    return humidity;
}

float readPh() {
    return analogRead(35) * 0.003418803;
}

float readSoilHumidity() {
    std::vector<int> arr;  
    
    // Leitura de 100 valores analógicos
    for(int i = 0; i < 100; i++) {
        int analogValue = 4095 - analogRead(34);  
        if (analogValue > 0) {
            arr.push_back(analogValue);  
        }
    }
    
    // Garante que temos ao menos 7 valores para aplicar o filtro
    if (arr.size() > 6) {
        // Ordena o vetor
        std::sort(arr.begin(), arr.end());

        // Remove os 3 maiores e os 3 menores valores
        arr.erase(arr.begin(), arr.begin() + 3);              // Remove os 3 menores
        arr.erase(arr.end() - 3, arr.end());                  // Remove os 3 maiores

        // Calcula a média dos valores restantes
        float sum = std::accumulate(arr.begin(), arr.end(), 0);
        float average = sum / arr.size();
        return average / 40.95;  // Ajuste para escala de umidade
    } else {
        return 0.0;  // Retorna 0 se o vetor tiver poucos elementos para uma média precisa
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
