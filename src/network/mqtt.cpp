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
  String message = String(value, 2);

  if (lastMessage != message) {
      xSemaphoreTake(lcdMutex, portMAX_DELAY);
      LCD.setCursor(0, 0);
      LCD.print(topic);
      LCD.setCursor(0, 1);
      LCD.print(message);
      xSemaphoreGive(lcdMutex);
      lastMessage = message;
  }

  xSemaphoreTake(mqttMutex, portMAX_DELAY);
  if (mqttClient.publish(topic, message.c_str(), false)) {
      Serial.print(topic);
      Serial.print(": ");
      Serial.println(value);
  } else {
      Serial.println(mqttClient.state());
      Serial.println("Failed to publish message.");
  }
  xSemaphoreGive(mqttMutex);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
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