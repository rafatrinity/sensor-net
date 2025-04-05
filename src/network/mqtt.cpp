// network/mqtt.cpp
#include "mqtt.hpp"
#include "config.hpp"
#include "mqttHandler.hpp"
#include "wifi.hpp" // Desnecessário aqui?

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient); // Ainda global

// Variável estática para guardar o roomTopic para o callback
String staticRoomTopic = "";

void setupMQTT(const MQTTConfig& config) { // <<< RECEBE CONFIG
  mqttClient.setServer(config.server, config.port); // <<< USA CONFIG
  mqttClient.setCallback(mqttCallback);

  // Guarda o room topic para o callback poder usá-lo (workaround)
  staticRoomTopic = String(config.roomTopic); // <<< GUARDA TOPIC

  // Garante a conexão inicial (pode ser feito dentro da tarefa também)
  ensureMQTTConnection(config);

  // Inscreve no tópico de controle
  xSemaphoreTake(mqttMutex, portMAX_DELAY);
  String controlTopic = staticRoomTopic + "/control"; // Usa a variável estática
  if(mqttClient.subscribe(controlTopic.c_str())){
      Serial.print("Subscribed to control topic: ");
      Serial.println(controlTopic);
  } else {
      Serial.print("Failed to subscribe to control topic: ");
      Serial.println(controlTopic);
  }
  xSemaphoreGive(mqttMutex);
}

void ensureMQTTConnection(const MQTTConfig& config) { // <<< RECEBE CONFIG
  const int loopDelay = 500;
  while (!mqttClient.connected()) {
      // Idealmente, esta função não deveria ser chamada por readSensors.
      // A tarefa manageMQTT deve ser a única responsável por manter a conexão.
      // Vamos assumir que manageMQTT chama isso.
      xSemaphoreTake(mqttMutex, portMAX_DELAY);
      Serial.println("Reconnecting to MQTT...");
      // Usa o clientId da configuração
      if (mqttClient.connect(config.clientId)) { // <<< USA CONFIG
          Serial.println("Reconnected");
          // Publica a mensagem 'devices' usando o roomTopic da configuração
          if (mqttClient.publish("devices", config.roomTopic)) { // <<< USA CONFIG
              Serial.print("Sent devices message: ");
              Serial.println(config.roomTopic);
          } else {
              Serial.println("Failed to send devices message");
          }
          // Reinscreve no tópico de controle após reconectar
          String controlTopic = String(config.roomTopic) + "/control";
          if(mqttClient.subscribe(controlTopic.c_str())){
             Serial.print("Re-subscribed to: "); Serial.println(controlTopic);
          } else {
             Serial.print("Failed re-subscribe to: "); Serial.println(controlTopic);
          }

      } else {
          Serial.print("Failed with state ");
          Serial.println(mqttClient.state());
          // Libera o mutex antes de esperar para evitar deadlock se outra tarefa precisar dele
          xSemaphoreGive(mqttMutex);
          vTaskDelay(loopDelay / portTICK_PERIOD_MS);
          // Retoma o mutex na próxima iteração do while
          continue; // Pula o xSemaphoreGive no final do loop normal
      }
      xSemaphoreGive(mqttMutex);
  }
}

// A função publishMQTTMessage foi corrigida anteriormente para remover acesso ao LCD.
// Não precisa da config diretamente, mas seu propósito geral deve ser revisado.
void publishMQTTMessage(const char* topic, float value) {
    static String lastMessage = "";
    String message = String(value, 2);

    xSemaphoreTake(mqttMutex, portMAX_DELAY);
    if (mqttClient.publish(topic, message.c_str(), false)) {
        Serial.print("Published MQTT ["); Serial.print(topic); Serial.print("]: ");
        Serial.println(message);
    } else {
        Serial.print("MQTT Publish Failed! State: "); Serial.println(mqttClient.state());
        Serial.print("  Topic: "); Serial.println(topic);
        Serial.print("  Message: "); Serial.println(message);
    }
    xSemaphoreGive(mqttMutex);
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Considere remover/ajustar
}


void manageMQTT(void *parameter){
  // Faz o cast do parâmetro para o tipo esperado
  const MQTTConfig* mqttConfig = static_cast<const MQTTConfig*>(parameter);
   if (mqttConfig == nullptr) {
        Serial.println("FATAL ERROR: MQTTTask received NULL config!");
        vTaskDelete(NULL); // Aborta a tarefa
        return;
    }

  // Chama setupMQTT passando a configuração
  setupMQTT(*mqttConfig); // <<< PASSA CONFIG

  const int loopDelay = 500;
  while (true) {
      if (WiFi.status() == WL_CONNECTED) {
          // Garante a conexão e processa mensagens MQTT
          ensureMQTTConnection(*mqttConfig); // <<< PASSA CONFIG
          // O loop precisa do mutex para ser seguro contra publicações simultâneas
          if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
             if(mqttClient.connected()) { // Verifica de novo após pegar o mutex
                 mqttClient.loop();
             }
             xSemaphoreGive(mqttMutex);
          } else {
              Serial.println("MQTTTask: Failed acquire mutex for loop.");
          }
      } else {
          Serial.println("WiFi disconnected, MQTT waiting...");
          // Não tenta reconectar MQTT se WiFi estiver caído
      }
      vTaskDelay(loopDelay / portTICK_PERIOD_MS);
  }
}