// mqttHandler.cpp
#include "mqttHandler.hpp"
#include "config.hpp" // Ainda necessário para TargetValues (via targets.hpp incluso)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstdio>

// --- Dependência Externa ---
extern TargetValues target; // Ainda global, precisa de refatoração (TargetManager/Fila)
extern String staticRoomTopic; // <<< USA A VARIÁVEL ESTÁTICA DECLARADA EM mqtt.cpp
// -------------------------

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

  // Usa a variável estática para construir o tópico de controle esperado
  String expectedControlTopic = staticRoomTopic + "/control";

  if (String(topic) == expectedControlTopic) { // <<< COMPARA COM TOPIC ESPERADO
      Serial.println("Processing control message...");
      JsonDocument doc; // Usa um JsonDocument local
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
          Serial.print("JSON deserialization failed: ");
          Serial.println(error.c_str());
          return;
      }

      // Atualiza target (AINDA PROBLEMÁTICO - acoplamento alto)
      // ... (lógica de atualização do target permanece a mesma por enquanto) ...
       if (!doc["airHumidity"].isNull() && doc["airHumidity"].is<double>()) {
          target.airHumidity = doc["airHumidity"].as<float>();
          Serial.print("Updated airHumidity to: ");
          Serial.println(target.airHumidity);
      }
      // ... resto das atualizações ...
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

// Declaração da variável estática para satisfazer o linker
// O valor será atribuído em setupMQTT.
extern String staticRoomTopic;