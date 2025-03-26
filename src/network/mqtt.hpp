#ifndef MQTT_HPP
#define MQTT_HPP

#include <PubSubClient.h>
#include <WiFi.h>

extern PubSubClient mqttClient;

void setupMQTT();
void ensureMQTTConnection();
void manageMQTT(void *parameter); 
void publishMQTTMessage(const char* topic, float value);

#endif