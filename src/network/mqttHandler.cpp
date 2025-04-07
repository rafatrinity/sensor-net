// src/network/mqttHandler.cpp (Modificado Temporariamente)
#include "mqttHandler.hpp"
//#include "config.hpp" // Não precisa mais de TargetValues daqui
#include "data/targetDataManager.hpp" // Incluir o novo manager (para referência futura)

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstdio>

// --- Dependência Externa ---
// extern TargetValues target; // <<< REMOVIDO/COMENTADO
extern String staticRoomTopic; // <<< Ainda aqui, será removido com MqttManager

// Instância do TargetDataManager (será injetada depois)
// GrowController::TargetDataManager* targetManagerInstance = nullptr; // Exemplo

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

  String expectedControlTopic = staticRoomTopic + "/control";

  if (String(topic) == expectedControlTopic) {
      Serial.println("Processing control message...");
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
          Serial.print("JSON deserialization failed: ");
          Serial.println(error.c_str());
          return;
      }

      // <<< LÓGICA DE ATUALIZAÇÃO COMENTADA/REMOVIDA >>>
      // Esta lógica agora pertence a TargetDataManager::updateTargetsFromJson
      // A chamada será feita em MqttManager::messageCallback
      /*
       if (targetManagerInstance) { // Exemplo de como seria chamado
           targetManagerInstance->updateTargetsFromJson(doc);
       } else {
           Serial.println("ERROR: TargetDataManager instance not available in callback!");
       }
      */
       Serial.println("Control message parsed (update logic deferred to TargetDataManager).");


      /* // Código antigo removido:
       if (!doc["airHumidity"].isNull() && doc["airHumidity"].is<double>()) {
          target.airHumidity = doc["airHumidity"].as<float>();
          Serial.print("Updated airHumidity to: ");
          Serial.println(target.airHumidity);
      }
       if (doc["lightOnTime"].is<String>()) {
          sscanf(doc["lightOnTime"], "%d:%d", &target.lightOnTime.tm_hour, &target.lightOnTime.tm_min);
          Serial.printf("Horário de ligar atualizado para %02d:%02d\n", target.lightOnTime.tm_hour, target.lightOnTime.tm_min);
      }
      if (doc["lightOffTime"].is<String>()) {
          sscanf(doc["lightOffTime"], "%d:%d", &target.lightOffTime.tm_hour, &target.lightOffTime.tm_min);
          Serial.printf("Horário de desligar atualizado para %02d:%02d\n", target.lightOffTime.tm_hour, target.lightOffTime.tm_min);
      }
      */
  }
}

// Declaração da variável estática para satisfazer o linker
// O valor será atribuído em setupMQTT.
// extern String staticRoomTopic; // <<< Mantido por enquanto