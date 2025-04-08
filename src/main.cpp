#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqttManager.hpp" // Incluir a nova classe MqttManager (precisa ser criada/refatorada)
#include "sensors/sensorManager.hpp"
#include "actuators/actuatorManager.hpp" // Assumindo que será refatorado para classe também
#include "data/targetDataManager.hpp"
#include "utils/timeService.hpp"       // Assumindo que será refatorado para classe
#include "ui/displayManager.hpp"
#include "utils/freeRTOSMutex.hpp" // Incluir se não estiver em outro header
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>

// --- Instâncias Principais (Managers) ---
AppConfig appConfig;
GrowController::TargetDataManager targetManager; // Já é uma classe
// Criar MqttManager, ActuatorManager, TimeService como classes mais tarde

// ===> Instanciar os managers refatorados <===
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS); // Definir os pinos/config no config.hpp ou board file
GrowController::SensorManager sensorMgr(appConfig.sensor, &displayMgr /*, &mqttMgr */); // Passa dependências
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr);

void setup()
{
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application (RAII Refactor) ---");

    // 1. Inicializa I2C ANTES de inicializar o DisplayManager
    Wire.begin(SDA, SCL); // Usa pinos do board config

    // 2. Inicializa Display Manager
    Serial.println("Initializing Display Manager...");
    if (!displayMgr.initialize()) {
        Serial.println("FATAL ERROR: Display Manager Initialization Failed!");
        // Lidar com erro: loop infinito, reiniciar, etc.
        while(1) { vTaskDelay(1000); }
    }
    displayMgr.showBooting();

    // 3. Inicializa Sensor Manager
    Serial.println("Initializing Sensor Manager...");
    if (!sensorMgr.initialize()) {
         Serial.println("FATAL ERROR: Sensor Manager Initialization Failed!");
         displayMgr.showError("Sensor Init");
         while(1) { vTaskDelay(1000); }
    }

    // 4. Inicializa Atuadores (Usando a classe)
    Serial.println("Initializing Actuator Manager...");
    if (!actuatorMgr.initialize()) {
         Serial.println("FATAL ERROR: Actuator Manager Initialization Failed!");
         displayMgr.showError("Actuator Init");
         while(1) { vTaskDelay(1000); }
    }

    // 5. Inicia Tarefa WiFi (Refatorar para classe WifiManager?)
    Serial.println("Starting WiFi Task...");
    
    WiFiTaskParams wifiParams = { &appConfig.wifi, &displayMgr };
    xTaskCreate(connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 1, NULL);
    
    // 6. Aguarda WiFi
    Serial.print("Waiting for WiFi connection...");
    displayMgr.showConnectingWiFi();
    while (WiFi.status() != WL_CONNECTED) {
         displayMgr.updateSpinner();
        vTaskDelay(pdMS_TO_TICKS(200)); // Aumentar um pouco o delay do spinner
    }
    Serial.println("\nWiFi Connected!");
    // displayMgr é atualizado por connectToWiFi via displayManagerShowWiFiConnected()

    // 7. Inicializa Time Service (Refatorar para classe TimeService)
    Serial.println("Initializing Time Service...");
    displayMgr.showNtpSyncing();
    if (!initializeTimeService(appConfig.time)) { // Chamada antiga
        Serial.println("ERROR: Failed to initialize Time Service!");
        displayMgr.showError("NTP Fail");
        // Continuar mesmo se NTP falhar?
    } else {
        displayMgr.showNtpSynced();
    }

    // 8. Inicializa e Inicia Tarefa MQTT (Refatorar para classe MqttManager)
    Serial.println("Initializing & Starting MQTT Manager...");
    displayMgr.showMqttConnecting();

    // 9. Inicia Tarefa de Leitura de Sensores (AGORA usando o manager)
     Serial.println("Starting Sensor Reading Task...");
     if (!sensorMgr.startSensorTask()) {
         Serial.println("FATAL ERROR: Failed to start Sensor Task!");
         displayMgr.showError("Task Sens Fail");
         while(1) { vTaskDelay(1000); }
     }


    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
     displayMgr.clear(); // Limpa mensagens de inicialização
     // O display será atualizado pela tarefa do SensorManager
}

void loop()
{
    // O loop principal pode ficar vazio ou fazer tarefas de baixa prioridade
    vTaskDelay(portMAX_DELAY); // Dorme indefinidamente
}