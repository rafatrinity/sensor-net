//mqttHandler.cpp
#include "mqttHandler.hpp"
#include "config.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstdio>


TargetValues target = {
  .airHumidity = 70.0f,
  .vpd = 1.0f,
  .soilHumidity = 66.0f,
  .temperature = 25.0f
};

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("===================================");
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String message;
  for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
  }
  Serial.print("Raw Message: ");
  Serial.println(message);

  if (String(topic) == String(appConfig.mqtt.roomTopic) + "/control") {
      Serial.println("Processing control message...");
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
          Serial.print("JSON deserialization failed: ");
          Serial.println(error.c_str());
          return;
      }

      // Atualizando valores existentes
      if (!doc["airHumidity"].isNull() && doc["airHumidity"].is<double>()) {
          target.airHumidity = doc["airHumidity"].as<float>();
          Serial.print("Updated airHumidity to: ");
          Serial.println(target.airHumidity);
      }
      
      if (!doc["vpd"].isNull() && doc["vpd"].is<double>()) {
          target.vpd = doc["vpd"].as<float>();
          Serial.print("Updated vpd to: ");
          Serial.println(target.vpd);
      }

      if (!doc["soilHumidity"].isNull() && doc["soilHumidity"].is<double>()) {
          target.soilHumidity = doc["soilHumidity"].as<float>();
          Serial.print("Updated soilHumidity to: ");
          Serial.println(target.soilHumidity);
      }

      if (!doc["temperature"].isNull() && doc["temperature"].is<double>()) {
          target.temperature = doc["temperature"].as<float>();
          Serial.print("Updated temperature to: ");
          Serial.println(target.temperature);
      }

      if (doc["lightOnTime"].is<String>()) {
          sscanf(doc["lightOnTime"], "%d:%d", &target.lightOnTime.tm_hour, &target.lightOnTime.tm_min);
          Serial.printf("Horário de ligar atualizado para %02d:%02d\n", target.lightOnTime.tm_hour, target.lightOnTime.tm_min);
      }
  
      if (doc["lightOffTime"].is<String>()) {
          sscanf(doc["lightOffTime"], "%d:%d", &target.lightOffTime.tm_hour, &target.lightOffTime.tm_min);
          Serial.printf("Horário de desligar atualizado para %02d:%02d\n", target.lightOffTime.tm_hour, target.lightOffTime.tm_min);
      }
  }
}
