// src/main.cpp
#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqttManager.hpp"
#include "sensors/sensorManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/targetDataManager.hpp"
#include "utils/timeService.hpp"
#include "ui/displayManager.hpp"

#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// --- Instâncias Principais (Managers Globais) ---
AppConfig appConfig;
GrowController::TargetDataManager targetManager;
GrowController::TimeService timeService;

// DisplayManager depende de timeService
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS, timeService);

// MqttManager depende de appConfig.mqtt e targetManager
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager);

// SensorManager depende de appConfig.sensor, displayMgr*, mqttMgr*
// displayMgr e mqttMgr são passados como ponteiros opcionais.
GrowController::SensorManager sensorMgr(appConfig.sensor, &displayMgr, &mqttMgr); // Instância global

// ActuatorManager depende de appConfig.gpioControl, targetManager, sensorMgr, timeService
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr, timeService); // Instância global

// Instâncias para comunicação BLE
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

// UUIDs para o serviço e característica BLE
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

// Função para ativar o modo de pareamento
void activatePairingMode() {
    Serial.println("Modo de pareamento BLE ativado.");

    // Inicializa o dispositivo BLE
    BLEDevice::init("SensorNet");
    pServer = BLEDevice::createServer();

    // Cria o serviço BLE
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Cria a característica BLE para receber credenciais
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE
                      );

    // Inicia o serviço BLE
    pService->start();

    // Inicia a publicidade BLE
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();

    Serial.println("Aguardando credenciais via BLE...");
}

/**
 * @brief Função principal de configuração da aplicação.
 */
void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application (RAII Refactor w/ Dep Injection Fixes) ---");

    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP); // Configura o botão como entrada com pull-up

    char ssid[32];
    char password[64];

    if (loadWiFiCredentials(ssid, password, sizeof(ssid))) {
        Serial.printf("Conectando ao Wi-Fi salvo: %s\n", ssid);
        WiFi.begin(ssid, password);
    } else {
        Serial.println("Nenhuma credencial Wi-Fi salva encontrada.");
    }

    // Verifica se o botão está pressionado na inicialização
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        activatePairingMode();
    }

    // 1. Inicializa I2C
    Wire.begin(SDA, SCL);

    // 2. Inicializa Display Manager (instância global)
    Serial.println("Initializing Display Manager...");
    displayOk = displayMgr.initialize();
    if (!displayOk) {
        Serial.println("FATAL: Display Manager Initialization Failed after retries!");
    } else {
        displayMgr.showBooting();
    }

    // 3. Configura MqttManager
    Serial.println("Setting up MQTT Manager...");
    mqttMgr.setup();
    mqttSetupOk = true;

    // --- Construção e Inicialização dos Managers Locais (se houver) ou uso dos Globais ---

    // 4. Inicializar Sensor Manager (instância global)
    Serial.println("Initializing Sensor Manager...");
    // Atualiza os ponteiros opcionais caso display/mqtt tenham falhado a inicialização
    // (Embora já tenham sido passados no construtor, isso garante consistência se
    // a lógica de passagem de ponteiro fosse feita aqui em vez de no construtor)
    // sensorMgr.setDisplayManager(displayOk ? &displayMgr : nullptr); // Exemplo se tivesse setters
    // sensorMgr.setMqttManager(mqttSetupOk ? &mqttMgr : nullptr);    // Exemplo se tivesse setters
    sensorsOk = sensorMgr.initialize(); // Usa instância global
    if (!sensorsOk) {
        Serial.println("ERROR: Sensor Manager Initialization Failed after retries!");
        if (displayOk) displayMgr.showError("Sensor Init Fail");
    }

    
    // 5. Inicializar Actuator Manager (instância global)
    Serial.println("Initializing Actuator Manager...");
    actuatorsOk = actuatorMgr.initialize(); // Usa instância global
    if (!actuatorsOk) {
        Serial.println("ERROR: Actuator Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Actuator Init Fail");
    }

    // --- Fim da inicialização dos managers ---

     // 6. Inicia Tarefa WiFi
     Serial.println("Starting WiFi Task...");
     WiFiTaskParams wifiParams = { &appConfig.wifi, (displayOk ? &displayMgr : nullptr) };
     BaseType_t wifiTaskResult = xTaskCreate( connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 1, NULL );
     if (wifiTaskResult != pdPASS) {
          Serial.println("FATAL ERROR: Failed to start WiFi Task!");
          if (displayOk) displayMgr.showError("Task WiFi Fail");
          while(1) { vTaskDelay(1000); }
     }
 

    // 7. Aguarda Conexão WiFi
    Serial.print("Waiting for WiFi connection...");
    if (displayOk) displayMgr.showConnectingWiFi();
    uint32_t wifiWaitStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiWaitStart < 60000)) {
        vTaskDelay(pdMS_TO_TICKS(200)); // Spinner atualizado pela task WiFi
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        Serial.println("\nWiFi Connected!");
    } else {
        wifiOk = false;
        Serial.println("\nERROR: WiFi Connection Failed (Timeout?)!");
        if (displayOk) displayMgr.showError("WiFi Fail");
    }

    // 8. Inicializa Time Service
    if (wifiOk) {
        if (displayOk) displayMgr.showNtpSyncing();
        timeOk = timeService.initialize(appConfig.time);
        if (!timeOk) {
            Serial.println("ERROR: Failed to configure Time Service!");
            if (displayOk) displayMgr.showError("NTP Cfg Fail");
        } else {
            struct tm timeinfo;
            if (timeService.getCurrentTime(timeinfo)) { // Verifica sync inicial
                if (displayOk) displayMgr.showNtpSynced(); // Usa displayMgr global
                Serial.println("Time Service initial sync successful.");
            } else {
                if (displayOk) displayMgr.showError("NTP Sync Fail"); // Usa displayMgr global
                Serial.println("WARN: Time Service configured, but initial sync failed. Will retry.");
            }
        }
    } else {
        Serial.println("Skipping Time Service initialization (No WiFi).\n");
        timeOk = false;
    }

    // 9. Inicia Tarefa MQTT (usa instância global mqttMgr)
    if (wifiOk && mqttSetupOk) {
        Serial.println("Starting MQTT Task...");
        if (displayOk) displayMgr.showMqttConnecting();
        BaseType_t mqttTaskResult = xTaskCreate( GrowController::MqttManager::taskRunner, "MQTTTask", 4096, &mqttMgr, 2, nullptr );
        if (mqttTaskResult != pdPASS) {
            Serial.println("ERROR: Failed to start MQTT Task!");
            if (displayOk) displayMgr.showError("Task MQTT Fail");
            mqttTaskOk = false;
        } else {
            mqttTaskOk = true;
        }
    } else {
        Serial.println("Skipping MQTT Task start (No WiFi or MQTT Setup failed).");
        mqttTaskOk = false;
    }

    // 10. Inicia Tarefa de Leitura de Sensores (usa instância global sensorMgr)
    if (sensorsOk) {
        Serial.println("Starting Sensor Reading Task...");
        // Passa o endereço da instância global sensorMgr
        sensorTaskOk = sensorMgr.startSensorTask(1, 4096);
        if (!sensorTaskOk) {
            Serial.println("ERROR: Failed to start Sensor Task!");
            if (displayOk) displayMgr.showError("Task Sens Fail");
        }
   } else {
        Serial.println("Skipping Sensor Task start (Sensor Init failed).");
        sensorTaskOk = false;
   }

    // 11. Inicia Tarefas de Controle de Atuadores (usa instância global actuatorMgr)
    bool canStartLightTask = actuatorsOk && timeOk;
    bool canStartHumidityTask = actuatorsOk && sensorsOk;
    if (actuatorsOk && (canStartLightTask || canStartHumidityTask)) {
        Serial.println("Starting Actuator Control Tasks...");
        actuatorTasksOk = actuatorMgr.startControlTasks(1, 1, 2560);
        if (!actuatorTasksOk) {
             Serial.println("ERROR: Failed to start one or both Actuator Tasks!");
             if (displayOk) displayMgr.showError("Task Act Fail");
        } else {
             Serial.println("Actuator Tasks started.");
        }
    } else {
        Serial.println("Skipping Actuator Tasks start (Actuators not initialized or critical dependencies missing/failed).");
        actuatorTasksOk = false;
    }

    // --- Setup Concluído ---
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("--- Setup Complete ---");
    // (Restante do log de status sem alterações)
    Serial.println("Initialization Status:");
    Serial.printf("  Display:     %s\n", displayOk ? "OK" : "FAILED");
    Serial.printf("  MQTT Setup:  %s\n", mqttSetupOk ? "OK" : "ASSUMED_OK");
    Serial.printf("  Sensors:     %s\n", sensorsOk ? "OK" : "FAILED");
    Serial.printf("  Actuators:   %s\n", actuatorsOk ? "OK" : "FAILED");
    Serial.printf("  WiFi:        %s\n", wifiOk ? "OK" : "FAILED");
    Serial.printf("  Time Config: %s\n", timeOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  MQTT Task:   %s\n", mqttTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Sensor Task: %s\n", sensorTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Actuator Tsk:%s\n", actuatorTasksOk ? "OK" : "FAILED/SKIPPED");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());

    if (displayOk) {
        bool systemDegraded = !sensorsOk || !actuatorsOk || !wifiOk || !timeOk || !mqttTaskOk || !sensorTaskOk || !actuatorTasksOk;
        if (systemDegraded) {
             displayMgr.printLine(0, "System Started");
             displayMgr.printLine(1, "Degraded Mode");
        } else {
            displayMgr.clear(); // Limpa se tudo OK
        }
    }
}

void loop() {
    // Verifica se o botão foi pressionado durante a execução
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
        activatePairingMode();
    }

    vTaskSuspend(NULL);
}