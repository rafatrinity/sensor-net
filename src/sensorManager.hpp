#ifndef SENSOR_MANAGER_HPP
#define SENSOR_MANAGER_HPP

#include <DHT.h>

// Declaração dos pinos dos sensores
extern DHT dht;

// Funções de leitura dos sensores
float readDistanceCM();
float readTemperature();
float readHumidity();
float readPh();
float readSoilHumidity();

// Funções de publicação de dados dos sensores
void publishDistanceData();
void publishTemperatureData();
void publishHumidityData();
void publishPhData();
void publishSoilHumidityData();

void initializeSensors();

#endif
