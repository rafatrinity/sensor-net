#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <PubSubClient.h>

struct TargetValues {
    float airHumidity;
    float vpd;
    float soilHumidity;
    float temperature;
    struct tm lightOnTime;
    struct tm lightOffTime;
};

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern TargetValues target;

void connectToWiFi(void*);
void ensureMQTTConnection();
void manageMQTT(void*);
void publishMQTTMessage(const char* topic, float value);
void spinner();
#endif
