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

void controlGpioByTime(int horaIni, int horaFim, int gpioPin);
int getCurrentHour();
void initializeNTP();

#endif
