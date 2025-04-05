//mqtt.cpp
#include "mqtt.hpp"
#include "config.hpp"        
#include "mqttHandler.hpp"  
#include "wifi.hpp"         

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
extern LiquidCrystal_I2C LCD;
PubSubClient mqttClient(espClient);

void setupMQTT() {
  mqttClient.setServer(appConfig.mqtt.server, appConfig.mqtt.port);
  mqttClient.setCallback(mqttCallback);
  ensureMQTTConnection();
  xSemaphoreTake(mqttMutex, portMAX_DELAY);
  if(mqttClient.subscribe((String(appConfig.mqtt.roomTopic) + "/control").c_str())){
      Serial.println("Subscribed to control topic");
  } else {
      Serial.println("Failed to subscribe to control topic");
  }
  xSemaphoreGive(mqttMutex);
}

void ensureMQTTConnection() {
  const int loopDelay = 500;
  while (!mqttClient.connected()) {
      xSemaphoreTake(mqttMutex, portMAX_DELAY);
      Serial.println("Reconnecting to MQTT...");
      if (mqttClient.connect("ESP32Client1")) {
          Serial.println("Reconnected");
          if (mqttClient.publish("devices", appConfig.mqtt.roomTopic)) {
              Serial.print("Sent devices message: ");
              Serial.println(appConfig.mqtt.roomTopic);
          } else {
              Serial.println("Failed to send devices message");
          }
      } else {
          Serial.print("Failed with state ");
          Serial.println(mqttClient.state());
          vTaskDelay(loopDelay / portTICK_PERIOD_MS);
      }
      xSemaphoreGive(mqttMutex);
  }
}

void publishMQTTMessage(const char* topic, float value) {
    static String lastMessage = "";
    String message = String(value, 2); // Mantém a lógica de converter float para string

    // --- INÍCIO DO BLOCO REMOVIDO ---
    // A lógica de evitar mensagens repetidas no display foi removida daqui
    // pois esta função NÃO deve mais interagir com o display.

    // Apenas publica a mensagem MQTT
    xSemaphoreTake(mqttMutex, portMAX_DELAY); // Usa o mutex CORRETO (mqttMutex)
    if (mqttClient.publish(topic, message.c_str(), false)) { // Publica a string 'message'
        Serial.print("Published MQTT ["); Serial.print(topic); Serial.print("]: ");
        Serial.println(message); // Log da mensagem publicada
    } else {
        Serial.print("MQTT Publish Failed! State: "); Serial.println(mqttClient.state());
        Serial.print("  Topic: "); Serial.println(topic);
        Serial.print("  Message: "); Serial.println(message);
    }
    xSemaphoreGive(mqttMutex); // Libera o mutex CORRETO
}

void manageMQTT(void *parameter){
  setupMQTT();
    const int loopDelay = 500;
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            ensureMQTTConnection();
            mqttClient.loop();
        } else {
            Serial.println("WiFi disconnected, waiting to reconnect...");
        }
        vTaskDelay(loopDelay / portTICK_PERIOD_MS);
    }
}