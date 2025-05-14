// network/wifi.cpp
#include "wifi.hpp"
#include "config.hpp"
#include "ui/displayManager.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

Preferences preferences;

void saveWiFiCredentials(const char* ssid, const char* password) {
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    Serial.println("Credenciais do Wi-Fi salvas com sucesso.");
}

bool loadWiFiCredentials(char* ssid, char* password, size_t maxLength) {
    preferences.begin("wifi", true);
    String storedSSID = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", "");
    preferences.end();

    if (storedSSID.length() > 0 && storedPassword.length() > 0) {
        strncpy(ssid, storedSSID.c_str(), maxLength);
        strncpy(password, storedPassword.c_str(), maxLength);
        return true;
    }
    return false;
}

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

    char loaded_ssid[32];
    char loaded_password[64];

    if (loadWiFiCredentials(loaded_ssid, loaded_password, sizeof(loaded_ssid))) {
        Serial.printf("Credenciais carregadas: SSID=%s\n", loaded_ssid);
        if (!WiFi.config(LOCAL_IP, GATEWAY_IP, SUBNET_MASK, PRIMARY_DNS_IP, SECONDARY_DNS_IP)) {
            Serial.println("Falha ao configurar IP estático, tentando DHCP...");
        }
        WiFi.begin(loaded_ssid, loaded_password);
    } else {
        Serial.println("Nenhuma credencial Wi-Fi salva encontrada. Tentando credenciais armazenadas no ESP32...");
        WiFi.begin(); // Tenta usar credenciais armazenadas no ESP32
    }

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
    } else {
        Serial.println("\nFalha ao conectar ao Wi-Fi, entrando em modo de configuração BLE...");
        displayMgr->showError("WiFi Fail");
        activatePairingMode(); // Função para entrar no modo de configuração BLE
    }

    Serial.println("WiFi Task finished.");
    vTaskDelete(NULL);
}