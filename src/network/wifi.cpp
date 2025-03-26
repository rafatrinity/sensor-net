#include "wifi.hpp"
#include "config.hpp"
#include "utils/spinner.hpp"

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

void connectToWiFi(void * parameter) {
  const uint32_t maxRetries = 20;
  const uint32_t retryDelay = 500;

  while (true) {
      Serial.println("Connecting to WiFi...");
      WiFi.begin(appConfig.wifi.ssid, appConfig.wifi.password);

      uint32_t attempt = 0;
      while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
          vTaskDelay(retryDelay / portTICK_PERIOD_MS);
          Serial.print(".");
          xSemaphoreTake(lcdMutex, portMAX_DELAY);
          spinner();
          xSemaphoreGive(lcdMutex);
          attempt++;
      }

      if (WiFi.status() == WL_CONNECTED) {
          Serial.println(WiFi.localIP());
          xSemaphoreTake(lcdMutex, portMAX_DELAY);
          LCD.setCursor(0, 0);
          LCD.print("IP:");
          LCD.print(WiFi.localIP().toString().c_str());
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          LCD.clear();
          xSemaphoreGive(lcdMutex);
          break;
      } else {
          Serial.println("\nFailed to connect to WiFi, retrying in 5 seconds...");
          vTaskDelay(5000 / portTICK_PERIOD_MS);
      }
  }
  vTaskDelete(NULL);
}
