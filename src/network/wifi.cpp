// network/wifi.cpp
#include "wifi.hpp"
#include "config.hpp"
#include "ui/displayManager.hpp"

#include <Arduino.h>
#include <WiFi.h>


void connectToWiFi(void *parameter) {
    WiFiTaskParams* params = static_cast<WiFiTaskParams*>(parameter);
    if (params == nullptr || params->wifiConfig == nullptr || params->displayMgr == nullptr) {
        Serial.println("FATAL ERROR: WiFiTask received NULL parameters!");
        vTaskDelete(NULL);
        return;
    }
    const WiFiConfig* wifiConfig = params->wifiConfig;
    GrowController::DisplayManager* displayMgr = params->displayMgr;

    const uint32_t maxRetries = 20;
    const uint32_t retryDelayMs = 500;
    displayMgr->showConnectingWiFi();

    while (true) {

        WiFi.begin(wifiConfig->ssid, wifiConfig->password);

        uint32_t attempt = 0;
        while (WiFi.status() != WL_CONNECTED && attempt < maxRetries) {
            vTaskDelay(pdMS_TO_TICKS(retryDelayMs));
            Serial.print(".");
            displayMgr->updateSpinner();
            attempt++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            String ipAddr = WiFi.localIP().toString();
            Serial.println(ipAddr.c_str());
            displayMgr->showWiFiConnected(ipAddr.c_str());
            break;
        } else {
            Serial.println("\nFailed to connect to WiFi, retrying in 5 seconds...");
            displayMgr->showError("WiFi Fail");
            vTaskDelay(pdMS_TO_TICKS(5000));
            displayMgr->clear();
        }
    }

    Serial.println("WiFi Task finished.");
    vTaskDelete(NULL);
}