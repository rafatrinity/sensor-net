#ifndef NETWORK_H
#define NETWORK_H
#define ROOM "01"

#include <WiFi.h>
#include <PubSubClient.h>

struct TargetValues {
    float airHumidity;
    float vpd;
    float soilHumidity;
    float temperature;
};

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern TargetValues target;

extern const char *ssid;
extern const char *password;
extern const char *mqtt_server;
extern const int mqtt_port;

void connectToWiFi(void*);
void ensureMQTTConnection();
void manageMQTT(void*);
void publishMQTTMessage(const char* topic, float value);
void spinner();
#endif
