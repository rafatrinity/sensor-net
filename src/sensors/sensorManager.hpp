#ifndef SENSOR_MANAGER_HPP
#define SENSOR_MANAGER_HPP

#include <DHT.h>

// Declaração dos pinos dos sensores
extern DHT dht;

// Funções de leitura dos sensores
float readTemperature();
float readHumidity();
float readSoilHumidity();

float calculateVpd(float tem, float hum);

void initializeSensors();

void lightControl(struct tm lightOn, struct tm lightOff, int gpioPin);
int getCurrentHour();
void initializeNTP();

#endif
