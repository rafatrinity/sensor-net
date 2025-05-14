// src/main.cpp
#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqttManager.hpp"
#include "sensors/sensorManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/targetDataManager.hpp"
#include "utils/timeService.hpp"
#include "ui/displayManager.hpp"
#include "network/webServerManager.hpp"

#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <LittleFS.h>

// Variáveis globais para sinalizar o recebimento de credenciais
volatile bool g_bleCredentialsReceived = false;
char g_receivedSsid[32];
char g_receivedPassword[64];

// --- Instâncias Principais (Managers Globais) ---
AppConfig appConfig; // appConfig deve ser preenchido com os valores de config.hpp
GrowController::TargetDataManager targetManager;
GrowController::TimeService timeService;

// Managers que dependem de outros devem ser declarados após suas dependências
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS, timeService);
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager);
GrowController::SensorManager sensorMgr(appConfig.sensor, &displayMgr, &mqttMgr); // Passar WebServerManager aqui se ele for chamar eventos SSE
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr, timeService);
GrowController::WebServerManager webServerManager(&sensorMgr, &targetManager, &actuatorMgr); // Agora sensorMgr e actuatorMgr existem


BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-123456789abc"

// --- Flags de Status da Inicialização ---
bool displayOk = false;
bool sensorsOk = false;
bool actuatorsOk = false;
bool wifiOk = false;
bool timeOk = false;
bool mqttSetupOk = false;
bool mqttTaskOk = false;
bool sensorTaskOk = false;
bool actuatorTasksOk = false;
bool littleFsOk = false; // Nova flag para LittleFS

// As variáveis de IP estático foram movidas para config.hpp

// Definição da função (estava faltando no seu wifi.cpp, mas é de main.cpp)

class CharacteristicCallbacks : public BLECharacteristicCallbacks { // Definição antes de ser usada
    void onWrite(BLECharacteristic *pChar) override {
        // ... (seu código do callback)
        Serial.println("Callback onWrite acionado.");
        std::string value = pChar->getValue();
        if (value.length() > 0) {
            Serial.println("BLE: Dados recebidos na característica:");
            Serial.println(value.c_str());
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value);
            if (error) {
                Serial.print("BLE Error: Falha ao analisar JSON: ");
                Serial.println(error.c_str());
                pChar->setValue("Erro: JSON invalido");
                return;
            }
            const char* ssid_ble = doc["ssid"]; // Renomeado para evitar conflito com char ssid[32]
            const char* password_ble = doc["password"]; // Renomeado
            if (ssid_ble && password_ble) {
                Serial.print("SSID Recebido via BLE: "); Serial.println(ssid_ble);
                Serial.print("Senha Recebida via BLE: "); Serial.println(password_ble);
                saveWiFiCredentials(ssid_ble, password_ble);
                Serial.println("Credenciais Wi-Fi salvas. Reiniciando o dispositivo para aplicar.");
                g_bleCredentialsReceived = true;
                strncpy(g_receivedSsid, ssid_ble, sizeof(g_receivedSsid) - 1);
                g_receivedSsid[sizeof(g_receivedSsid) - 1] = '\0';
                strncpy(g_receivedPassword, password_ble, sizeof(g_receivedPassword) - 1);
                g_receivedPassword[sizeof(g_receivedPassword) - 1] = '\0';
                ESP.restart();
            } else {
                Serial.println("BLE Error: SSID ou senha ausentes nos dados JSON recebidos.");
                pChar->setValue("Erro: Dados incompletos");
            }
        } else {
            Serial.println("BLE: Nenhum dado recebido na característica.");
        }
    }
};

void activatePairingMode() {
    Serial.println("Modo de pareamento BLE ativado.");
    // ... (resto da sua função activatePairingMode)
    BLEDevice::init("SensorNet");
    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE
                      );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks()); // CharacteristicCallbacks precisa estar definido antes
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setKeySize(16);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    Serial.println("Aguardando credenciais via BLE... (Callback configurado)");
    if (displayOk) {
        displayMgr.clear();
        displayMgr.printLine(0, "Modo Config BLE");
        displayMgr.printLine(1, "Envie WiFi Creds");
    }
}


void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);

    // 1. Inicializa I2C
    Wire.begin(SDA, SCL);

    // 2. Inicializa Display Manager
    Serial.println("Initializing Display Manager...");
    displayOk = displayMgr.initialize();
    if (!displayOk) Serial.println("FATAL: Display Manager Initialization Failed!");
    else displayMgr.showBooting();

    // 3. Inicializa LittleFS
    Serial.println("Initializing LittleFS...");
    if (!LittleFS.begin(true)) { // true para formatar se não conseguir montar
        Serial.println("LittleFS Mount Failed!");
        if (displayOk) displayMgr.showError("FS Mount Fail");
        littleFsOk = false;
    } else {
        Serial.println("LittleFS Mounted.");
        littleFsOk = true;
        // Opcional: Listar arquivos para depuração
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
            Serial.printf("  FILE: /%s (Size: %lu)\n", file.name(), file.size());
            file.close();
            file = root.openNextFile();
        }
        root.close();
    }

    // Verifica se o botão está pressionado na inicialização ANTES de iniciar a tarefa WiFi
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        // Se LittleFS falhou, o BLE ainda pode funcionar para obter credenciais
        activatePairingMode(); // Esta função agora está definida globalmente
        // O dispositivo pode reiniciar dentro de activatePairingMode se credenciais forem recebidas.
        // Se não, a execução continua.
    }

    // 4. Inicia Tarefa WiFi (que agora lida com config de IP e begin)
    Serial.println("Starting WiFi Task...");
    WiFiTaskParams wifiParams = { &appConfig.wifi, (displayOk ? &displayMgr : nullptr) };
    BaseType_t wifiTaskResult = xTaskCreate( connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 1, NULL );
    if (wifiTaskResult != pdPASS) {
          Serial.println("FATAL ERROR: Failed to start WiFi Task!");
          if (displayOk) displayMgr.showError("Task WiFi Fail");
          while(1) { vTaskDelay(1000); } // Trava se a tarefa WiFi não puder ser criada
    }

    // 5. Aguarda Conexão WiFi
    Serial.print("Waiting for WiFi connection from task...");
    if (displayOk) displayMgr.showConnectingWiFi(); // A tarefa WiFi também pode chamar isso
    uint32_t wifiWaitStart = millis();
    // A tarefa connectToWiFi agora é responsável por definir WiFi.status()
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiWaitStart < 60000)) { // Timeout de 60s
        vTaskDelay(pdMS_TO_TICKS(200));
        // O spinner é atualizado pela tarefa WiFi
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        // O display é atualizado pela tarefa WiFi
    } else {
        wifiOk = false;
        Serial.println("\nERROR: WiFi Connection Failed (Timeout or from task)!");
        if (displayOk) displayMgr.showError("WiFi Fail");
        // A tarefa connectToWiFi pode ter chamado activatePairingMode se falhou
    }

    // 6. Inicializa WebServer (APÓS WiFi e LittleFS)
    if (wifiOk && littleFsOk) {
        Serial.println("Starting Web Server...");
        webServerManager.begin();
        if (displayOk) {
             displayMgr.printLine(1, WiFi.localIP().toString().c_str()); // Mostra IP no display
        }
    } else {
        Serial.println("Skipping Web Server start (No WiFi or LittleFS not mounted).");
        if (displayOk && !littleFsOk) displayMgr.showError("Web Srv Skip");
    }

    // 7. Configura MqttManager
    Serial.println("Setting up MQTT Manager...");
    mqttMgr.setup(); // Não depende de conexão, apenas configura
    mqttSetupOk = true;

    // 8. Inicializar Sensor Manager
    Serial.println("Initializing Sensor Manager...");
    sensorsOk = sensorMgr.initialize();
    if (!sensorsOk) {
        Serial.println("ERROR: Sensor Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Sensor Init Fail");
    }

    // 9. Inicializar Actuator Manager
    Serial.println("Initializing Actuator Manager...");
    actuatorsOk = actuatorMgr.initialize();
    if (!actuatorsOk) {
        Serial.println("ERROR: Actuator Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Actuator Init Fail");
    }

    // 10. Inicializa Time Service (APÓS WiFi)
    if (wifiOk) {
        if (displayOk) displayMgr.showNtpSyncing();
        timeOk = timeService.initialize(appConfig.time);
        if (!timeOk) {
            Serial.println("ERROR: Failed to configure Time Service!");
            if (displayOk) displayMgr.showError("NTP Cfg Fail");
        } else {
            struct tm timeinfo;
            if (timeService.getCurrentTime(timeinfo)) {
                if (displayOk) displayMgr.showNtpSynced();
                Serial.println("Time Service initial sync successful.");
            } else {
                if (displayOk) displayMgr.showError("NTP Sync Fail");
                Serial.println("WARN: Time Service configured, but initial sync failed.");
            }
        }
    } else {
        Serial.println("Skipping Time Service initialization (No WiFi).");
        timeOk = false;
    }

    // 11. Inicia Tarefa MQTT
    if (wifiOk && mqttSetupOk) {
        Serial.println("Starting MQTT Task...");
        if (displayOk) displayMgr.showMqttConnecting();
        BaseType_t mqttTaskResult = xTaskCreate( GrowController::MqttManager::taskRunner, "MQTTTask", 4096, &mqttMgr, 2, nullptr );
        mqttTaskOk = (mqttTaskResult == pdPASS);
        if (!mqttTaskOk) {
            Serial.println("ERROR: Failed to start MQTT Task!");
            if (displayOk) displayMgr.showError("Task MQTT Fail");
        }
    } else {
        Serial.println("Skipping MQTT Task start (No WiFi or MQTT Setup failed).");
        mqttTaskOk = false;
    }

    // 12. Inicia Tarefa de Leitura de Sensores
    if (sensorsOk) {
        Serial.println("Starting Sensor Reading Task...");
        sensorTaskOk = sensorMgr.startSensorTask(1, 4096); // Prioridade 1
        if (!sensorTaskOk) {
            Serial.println("ERROR: Failed to start Sensor Task!");
            if (displayOk) displayMgr.showError("Task Sens Fail");
        }
   } else {
        Serial.println("Skipping Sensor Task start (Sensor Init failed).");
        sensorTaskOk = false;
   }

    // 13. Inicia Tarefas de Controle de Atuadores
    bool canStartLightTask = actuatorsOk && timeOk; // Precisa de hora para a luz
    bool canStartHumidityTask = actuatorsOk && sensorsOk; // Precisa de sensor de umidade do ar
    if (actuatorsOk && (canStartLightTask || canStartHumidityTask)) {
        Serial.println("Starting Actuator Control Tasks...");
        actuatorTasksOk = actuatorMgr.startControlTasks(1, 1, 2560); // Prioridade 1
        if (!actuatorTasksOk) {
             Serial.println("ERROR: Failed to start one or both Actuator Tasks!");
             if (displayOk) displayMgr.showError("Task Act Fail");
        } else {
             Serial.println("Actuator Tasks started.");
        }
    } else {
        Serial.println("Skipping Actuator Tasks start (Actuators/Sensors/Time not ready).");
        actuatorTasksOk = false;
    }

    Serial.println("--- Setup Complete ---");
    Serial.println("Initialization Status:");
    Serial.printf("  Display:     %s\n", displayOk ? "OK" : "FAILED");
    Serial.printf("  LittleFS:    %s\n", littleFsOk ? "OK" : "FAILED");
    Serial.printf("  WiFi:        %s (IP: %s)\n", wifiOk ? "OK" : "FAILED", WiFi.localIP().toString().c_str());
    Serial.printf("  WebServer:   %s\n", (wifiOk && littleFsOk) ? "RUNNING" : "SKIPPED");
    Serial.printf("  MQTT Setup:  %s\n", mqttSetupOk ? "OK" : "FAILED");
    Serial.printf("  Sensors:     %s\n", sensorsOk ? "OK" : "FAILED");
    Serial.printf("  Actuators:   %s\n", actuatorsOk ? "OK" : "FAILED");
    Serial.printf("  Time Config: %s\n", timeOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  MQTT Task:   %s\n", mqttTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Sensor Task: %s\n", sensorTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Actuator Tsk:%s\n", actuatorTasksOk ? "OK" : "FAILED/SKIPPED");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());

    if (displayOk) {
        bool systemDegraded = !littleFsOk || !sensorsOk || !actuatorsOk || !wifiOk || !timeOk || !mqttTaskOk || !sensorTaskOk || !actuatorTasksOk;
        if (systemDegraded && !(digitalRead(PAIRING_BUTTON_PIN) == LOW)) { // Não mostrar degradado se estiver em modo de pareamento
             displayMgr.printLine(0, "System Started");
             displayMgr.printLine(1, wifiOk ? WiFi.localIP().toString().c_str() : "Degraded Mode");
        } else if (wifiOk && littleFsOk && !systemDegraded) {
            // Se tudo OK e web server rodando, o display pode mostrar o IP e dados dos sensores
            // A tarefa de sensores já atualiza o display.
             displayMgr.printLine(0, "System OK");
             displayMgr.printLine(1, WiFi.localIP().toString().c_str());
        }
    }
}

// Loop principal - Chamada para enviar eventos SSE
void loop() {
    // Verifica se o botão foi pressionado durante a execução
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        // Adicionar um debounce simples para evitar múltiplas ativações
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce delay
        if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
            activatePairingMode();
            // A função activatePairingMode pode reiniciar o dispositivo.
            // Se não reiniciar, o loop continua.
        }
    }

    // Enviar eventos SSE periodicamente se o servidor web estiver ativo
    // Esta é uma forma simples. Idealmente, os eventos seriam enviados
    // apenas quando os dados mudam, disparados pelos respectivos managers.
    static unsigned long lastSseSendTime = 0;
    if (wifiOk && littleFsOk && (millis() - lastSseSendTime > 2000)) { // Envia a cada 2 segundos
        webServerManager.sendSensorUpdateEvent();
        webServerManager.sendStatusUpdateEvent();
        lastSseSendTime = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Reduz a carga da CPU no loop principal
    // vTaskSuspend(NULL); // Se o loop principal não tiver mais nada para fazer
}