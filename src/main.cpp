// src/main.cpp
#include "config.hpp"
#include "network/wifi.hpp"          // Para connectToWiFi e WiFiTaskParams
#include "network/mqttManager.hpp"   // Classe MqttManager refatorada
#include "sensors/sensorManager.hpp" // Classe SensorManager refatorada
#include "actuators/actuatorManager.hpp"// Classe ActuatorManager refatorada
#include "data/targetDataManager.hpp" // Classe TargetDataManager
#include "utils/timeService.hpp"      // Funções C-style (a refatorar)
#include "ui/displayManager.hpp"      // Classe DisplayManager refatorada
#include "utils/freeRTOSMutex.hpp"  // Wrapper Mutex RAII

#include <WiFi.h>                 // Para WiFi.status() e WL_CONNECTED
#include <Wire.h>                 // Para I2C
#include <ArduinoJson.h>          // Usado em alguns managers

// --- Instâncias Principais (Managers) ---
// Ordem pode ser importante para injeção de dependência
AppConfig appConfig;
GrowController::TargetDataManager targetManager;
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS); // Usa defines dos board files
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager); // Depende de TargetDataManager
GrowController::SensorManager sensorMgr(appConfig.sensor, &displayMgr, &mqttMgr); // Depende de DisplayManager e MqttManager
GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr); // Depende de TargetDataManager e SensorManager
// TimeService ainda é C-style

/**
 * @brief Função principal de configuração da aplicação.
 * Inicializa hardware, managers e cria as tarefas FreeRTOS.
 */
void setup()
{
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application (RAII Refactor) ---");

    // 1. Inicializa I2C (necessário para o Display)
    Wire.begin(SDA, SCL); // Usa pinos definidos em board_*.hpp via config.hpp

    // 2. Inicializa Display Manager
    Serial.println("Initializing Display Manager...");
    if (!displayMgr.initialize()) {
        Serial.println("FATAL ERROR: Display Manager Initialization Failed!");
        // Loop infinito ou reinicialização seria apropriado aqui
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

    // 4. Inicializa Actuator Manager
    Serial.println("Initializing Actuator Manager...");
    if (!actuatorMgr.initialize()) {
         Serial.println("FATAL ERROR: Actuator Manager Initialization Failed!");
         displayMgr.showError("Actuator Init");
         while(1) { vTaskDelay(1000); }
    }

    // 5. Configura MqttManager (antes de iniciar tarefas que dependem dele)
    Serial.println("Setting up MQTT Manager...");
    mqttMgr.setup(); // Configura cliente, servidor, callback
    // Adicionar verificação de sucesso se setup() for modificado para retornar bool

    // 6. Inicia Tarefa WiFi
    Serial.println("Starting WiFi Task...");
    WiFiTaskParams wifiParams = { &appConfig.wifi, &displayMgr }; // Usa struct definida em wifi.hpp
    BaseType_t wifiTaskResult = xTaskCreate(
        connectToWiFi,   // Função da tarefa
        "WiFiTask",      // Nome
        4096,            // Stack size
        (void *)&wifiParams, // Parâmetro (struct)
        1,               // Prioridade
        NULL             // Handle (opcional)
    );
    if (wifiTaskResult != pdPASS) {
         Serial.println("FATAL ERROR: Failed to start WiFi Task!");
         displayMgr.showError("Task WiFi Fail");
         while(1) { vTaskDelay(1000); }
    }

    // 7. Aguarda Conexão WiFi (Bloqueia setup até conectar)
    Serial.print("Waiting for WiFi connection...");
    displayMgr.showConnectingWiFi(); // Atualiza display
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    Serial.println("\nWiFi Connected!");
    // O display é atualizado pela própria tarefa connectToWiFi ao sucesso

    // 8. Inicializa Time Service (após WiFi conectar)
    Serial.println("Initializing Time Service...");
    displayMgr.showNtpSyncing();
    // Ainda usando a função C-style - refatorar para classe TimeService depois
    if (!initializeTimeService(appConfig.time)) {
        Serial.println("ERROR: Failed to initialize Time Service!");
        displayMgr.showError("NTP Fail");
        // Pode continuar mesmo com falha no NTP? Depende do requisito.
    } else {
        displayMgr.showNtpSynced();
    }

    // 9. Inicia Tarefa MQTT (Usando MqttManager)
    Serial.println("Starting MQTT Task...");
    displayMgr.showMqttConnecting(); // Atualiza display
    BaseType_t mqttTaskResult = xTaskCreate(
        GrowController::MqttManager::taskRunner, // Método estático da classe
        "MQTTTask",        // Nome
        4096,              // Stack size (ajuste se necessário)
        &mqttMgr,          // Passa ponteiro para a instância MqttManager
        2,                 // Prioridade (maior que WiFi/Sensores?)
        nullptr            // Handle (pode ser armazenado em mqttMgr.taskHandle)
    );
     if (mqttTaskResult != pdPASS) {
        Serial.println("FATAL ERROR: Failed to start MQTT Task!");
        displayMgr.showError("Task MQTT Fail");
        while(1) { vTaskDelay(1000); }
    }

    // 10. Inicia Tarefa de Leitura de Sensores (Usando SensorManager)
     Serial.println("Starting Sensor Reading Task...");
     if (!sensorMgr.startSensorTask(1 /* Prioridade */, 4096 /* Stack */)) { // Passa prioridade/stack
         Serial.println("FATAL ERROR: Failed to start Sensor Task!");
         displayMgr.showError("Task Sens Fail");
         while(1) { vTaskDelay(1000); }
     }

    // 11. Inicia Tarefas de Controle de Atuadores (Usando ActuatorManager)
    Serial.println("Starting Actuator Control Tasks...");
    if (!actuatorMgr.startControlTasks(1 /* Prioridade Luz */, 1 /* Prioridade Umid */, 2560 /* Stack */)) {
         Serial.println("FATAL ERROR: Failed to start Actuator Tasks!");
         displayMgr.showError("Task Act Fail");
         while(1) { vTaskDelay(1000); }
    }

    // --- Setup Concluído ---
    vTaskDelay(pdMS_TO_TICKS(500)); // Pequeno delay para estabilizar
    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
    displayMgr.clear(); // Limpa mensagens de inicialização
    // O display agora será atualizado principalmente pela tarefa do SensorManager
}

/**
 * @brief Loop principal do Arduino.
 * No nosso caso com FreeRTOS, pode ficar vazio ou realizar tarefas de baixa prioridade.
 */
void loop()
{
    // Suspende a tarefa loop() indefinidamente, pois as outras tarefas cuidam da lógica.
    vTaskDelay(portMAX_DELAY);
}