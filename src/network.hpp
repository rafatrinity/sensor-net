#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <PubSubClient.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern float target;

void connectToWiFi(void*);
void ensureMQTTConnection();
void manageMQTT(void*);
void publishMQTTMessage(const char* topic, float value);
void spinner();

extern const char *ssid;
extern const char *password;
extern const char *mqtt_server;
extern const int mqtt_port;

#endif
