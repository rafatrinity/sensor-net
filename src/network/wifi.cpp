// network/wifi.cpp
#include "wifi.hpp"
#include "config.hpp"
#include "ui/displayManager.hpp"

#include <Arduino.h>
#include <WiFi.h>


void connectToWiFi(void * parameter) {
    // Faz o cast do parâmetro para o tipo esperado
    const WiFiConfig* wifiConfig = static_cast<const WiFiConfig*>(parameter);
     if (wifiConfig == nullptr) {
        Serial.println("FATAL ERROR: WiFiTask received NULL config!");
        vTaskDelete(NULL); // Aborta a tarefa
        return;
    }

    const uint32_t maxRetries = 20;
    const uint32_t retryDelayMs = 500;

    while (true) {
        Serial.println("Connecting to WiFi...");
        displayManagerShowConnectingWiFi();

        // Usa a configuração recebida via parâmetro
        WiFi.begin(wifiConfig->ssid, wifiConfig->password); // <<< USA CONFIG

        uint32_t attempt = 0;
        while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
            vTaskDelay(pdMS_TO_TICKS(retryDelayMs));
            Serial.print(".");
            displayManagerUpdateSpinner();
            attempt++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            String ipAddr = WiFi.localIP().toString();
            Serial.println(ipAddr.c_str());
            displayManagerShowWiFiConnected(ipAddr.c_str());
            break;
        } else {
            Serial.println("\nFailed to connect to WiFi, retrying in 5 seconds...");
            displayManagerShowError("WiFi Fail");
            vTaskDelay(pdMS_TO_TICKS(5000));
            displayManagerClear();
        }
    }

    Serial.println("WiFi Task finished.");
    vTaskDelete(NULL);
}