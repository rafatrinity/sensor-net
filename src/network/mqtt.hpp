// network/mqtt.hpp
#ifndef MQTT_HPP
#define MQTT_HPP

#include <PubSubClient.h>
#include <WiFi.h>
#include "config.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern PubSubClient mqttClient; // Ainda global por enquanto
extern SemaphoreHandle_t mqttMutex;

void setupMQTT(const MQTTConfig& config);
void ensureMQTTConnection(const MQTTConfig& config);
void manageMQTT(void *parameter);
void publishMQTTMessage(const char* topic, float value);

#endif