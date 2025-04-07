#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H
#include "config.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstdio>

void mqttCallback(char* topic, byte* payload, unsigned int length);
#endif