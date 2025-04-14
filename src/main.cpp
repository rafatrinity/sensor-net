// src/main.cpp
#include "config.hpp"
#include "network/wifi.hpp"          // Para connectToWiFi e WiFiTaskParams
#include "network/mqttManager.hpp"   // Classe MqttManager refatorada
#include "sensors/sensorManager.hpp" // Classe SensorManager refatorada
#include "actuators/actuatorManager.hpp"// Classe ActuatorManager refatorada
#include "data/targetDataManager.hpp" // Classe TargetDataManager
#include "utils/timeService.hpp"      // Funções C-style (a refatorar)
#include "ui/displayManager.hpp"      // Classe DisplayManager refatorada

#include <WiFi.h>                 // Para WiFi.status() e WL_CONNECTED
#include <Wire.h>                 // Para I2C
#include <ArduinoJson.h>          // Usado em alguns managers

// --- Instâncias Principais (Managers - Construídos no Setup ou Globalmente se sem deps complexas) ---
AppConfig appConfig;
GrowController::TargetDataManager targetManager; // OK Global - Sem deps complexas no construtor
GrowController::DisplayManager displayMgr(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS); // OK Global - Deps simples no construtor
GrowController::MqttManager mqttMgr(appConfig.mqtt, targetManager); // OK Global - Depende de targetManager global
// TimeService ainda é C-style

// --- Flags de Status da Inicialização ---
bool displayOk = false;
bool sensorsOk = false;
bool actuatorsOk = false;
bool wifiOk = false;
bool timeOk = false;
bool mqttSetupOk = false; // Para setup() do MQTT
bool mqttTaskOk = false;
bool sensorTaskOk = false;
bool actuatorTasksOk = false;

/**
 * @brief Função principal de configuração da aplicação.
 * Inicializa hardware, managers e cria as tarefas FreeRTOS.
 */
 void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application (RAII Refactor w/ Retries) ---");

    // 1. Inicializa I2C (necessário para o Display)
    Wire.begin(SDA, SCL); // Usa defines dos board files

    // 2. Inicializa Display Manager
    Serial.println("Initializing Display Manager...");
    displayOk = displayMgr.initialize();
    if (!displayOk) {
        Serial.println("FATAL: Display Manager Initialization Failed after retries!");
        // Continuar sem display
    } else {
        displayMgr.showBooting();
    }

    // 3. Configura MqttManager (antes de construir SensorManager que depende dele)
    Serial.println("Setting up MQTT Manager...");
    mqttMgr.setup(); // Configura cliente, servidor, callback
    mqttSetupOk = true; // Assumindo que setup não falha criticamente por enquanto

    // --- Construção e Inicialização dos Managers com Dependências ---
    // SensorManager precisa de DisplayManager* e MqttManager*
    // ActuatorManager precisa de TargetDataManager& e SensorManager&

    // 4. Construir e Inicializar Sensor Manager AGORA
    Serial.println("Initializing Sensor Manager...");
    GrowController::SensorManager sensorMgr(appConfig.sensor,
                                            (displayOk ? &displayMgr : nullptr),
                                            (mqttSetupOk ? &mqttMgr : nullptr)); // Passa ponteiros no construtor
    sensorsOk = sensorMgr.initialize(); // Tenta inicializar hardware do sensor
    if (!sensorsOk) {
        Serial.println("ERROR: Sensor Manager Initialization Failed after retries!");
        if (displayOk) displayMgr.showError("Sensor Init Fail");
        // Continuar sem sensores? O controle baseado em sensor não funcionará.
    }

    // 5. Construir e Inicializar Actuator Manager AGORA (depende de sensorMgr)
    Serial.println("Initializing Actuator Manager...");
    GrowController::ActuatorManager actuatorMgr(appConfig.gpioControl, targetManager, sensorMgr); // Passa referências no construtor
    actuatorsOk = actuatorMgr.initialize(); // Tenta inicializar GPIOs dos atuadores
    if (!actuatorsOk) {
        Serial.println("ERROR: Actuator Manager Initialization Failed!");
        if (displayOk) displayMgr.showError("Actuator Init Fail");
        // Continuar sem atuadores? O controle não funcionará.
    }

    // --- Fim da construção/inicialização dos managers principais ---

    // 6. Inicia Tarefa WiFi
    Serial.println("Starting WiFi Task...");
    // Passa displayMgr somente se OK
    WiFiTaskParams wifiParams = { &appConfig.wifi, (displayOk ? &displayMgr : nullptr) };
    BaseType_t wifiTaskResult = xTaskCreate(
        connectToWiFi, "WiFiTask", 4096, (void *)&wifiParams, 1, NULL );
    if (wifiTaskResult != pdPASS) {
         Serial.println("FATAL ERROR: Failed to start WiFi Task!");
         if (displayOk) displayMgr.showError("Task WiFi Fail");
         while(1) { vTaskDelay(1000); } // Parada crítica
    }

    // 7. Aguarda Conexão WiFi
    Serial.print("Waiting for WiFi connection...");
    if (displayOk) displayMgr.showConnectingWiFi();
    uint32_t wifiWaitStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiWaitStart < 60000)) { // Timeout de 60s
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        Serial.println("\nWiFi Connected!");
        // Display atualizado pela tarefa connectToWiFi
    } else {
        wifiOk = false;
        Serial.println("\nERROR: WiFi Connection Failed (Timeout?)!");
        if (displayOk) displayMgr.showError("WiFi Fail");
        // Continuar sem WiFi? NTP e MQTT não funcionarão.
    }

    // 8. Inicializa Time Service (SOMENTE se WiFi conectou)
    if (wifiOk) {
        Serial.println("Initializing Time Service...");
        if (displayOk) displayMgr.showNtpSyncing();
        timeOk = initializeTimeService(appConfig.time);
        if (!timeOk) {
            Serial.println("ERROR: Failed to initialize Time Service (NTP Sync Failed?)!");
            if (displayOk) displayMgr.showError("NTP Fail");
            // Continuar sem hora exata? Controle de luz pode falhar.
        } else {
             if (displayOk) displayMgr.showNtpSynced();
        }
    } else {
        Serial.println("Skipping Time Service initialization (No WiFi).");
        timeOk = false;
    }

    // 9. Inicia Tarefa MQTT (SOMENTE se WiFi conectou e setup MQTT foi ok)
    if (wifiOk && mqttSetupOk) {
        Serial.println("Starting MQTT Task...");
        if (displayOk) displayMgr.showMqttConnecting();
        // Passa o endereço do mqttMgr global
        BaseType_t mqttTaskResult = xTaskCreate(
            GrowController::MqttManager::taskRunner, "MQTTTask", 4096, &mqttMgr, 2, nullptr );
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

    // 10. Inicia Tarefa de Leitura de Sensores (SOMENTE se sensoresOk)
    if (sensorsOk) {
         Serial.println("Starting Sensor Reading Task...");
         // Passa o endereço do sensorMgr local do setup
         sensorTaskOk = sensorMgr.startSensorTask(1, 4096);
         if (!sensorTaskOk) {
             Serial.println("ERROR: Failed to start Sensor Task!");
             if (displayOk) displayMgr.showError("Task Sens Fail");
         }
    } else {
         Serial.println("Skipping Sensor Task start (Sensor Init failed).");
         sensorTaskOk = false;
    }

    // 11. Inicia Tarefas de Controle de Atuadores (SOMENTE se actuatorsOk e dependências atendidas)
    bool canStartLightTask = actuatorsOk && timeOk; // Controle de Luz precisa de hora
    bool canStartHumidityTask = actuatorsOk && sensorsOk; // Controle de Umidade precisa de sensores (a task buscará os dados)

    if (actuatorsOk && (canStartLightTask || canStartHumidityTask)) { // Tenta iniciar se atuadores OK e pelo menos uma condição de controle é possível
        Serial.println("Starting Actuator Control Tasks...");
        // Passa o endereço do actuatorMgr local do setup
        // Idealmente, startControlTasks poderia receber flags para iniciar apenas tarefas viáveis,
        // mas por enquanto, inicia ambas se actuatorsOk. As tarefas internas devem lidar
        // com a falta temporária de dados (ex: hora não sincronizada, sensor NAN).
        actuatorTasksOk = actuatorMgr.startControlTasks(1, 1, 2560);
        if (!actuatorTasksOk) {
             Serial.println("ERROR: Failed to start one or both Actuator Tasks!");
             if (displayOk) displayMgr.showError("Task Act Fail");
        } else {
             Serial.println("Actuator Tasks started (individual tasks might wait for dependencies like time/sensor data).");
        }
    } else {
        Serial.println("Skipping Actuator Tasks start (Actuators not initialized or critical dependencies missing).");
        actuatorTasksOk = false;
    }


    // --- Setup Concluído ---
    vTaskDelay(pdMS_TO_TICKS(500)); // Pequeno delay para mensagens de log finalizarem
    Serial.println("--- Setup Complete ---");
    Serial.println("Initialization Status:");
    Serial.printf("  Display:     %s\n", displayOk ? "OK" : "FAILED");
    Serial.printf("  MQTT Setup:  %s\n", mqttSetupOk ? "OK" : "ASSUMED_OK"); // Mudança aqui, pois não verificamos falha
    Serial.printf("  Sensors:     %s\n", sensorsOk ? "OK" : "FAILED");
    Serial.printf("  Actuators:   %s\n", actuatorsOk ? "OK" : "FAILED");
    Serial.printf("  WiFi:        %s\n", wifiOk ? "OK" : "FAILED");
    Serial.printf("  Time:        %s\n", timeOk ? "OK" : "FAILED");
    Serial.printf("  MQTT Task:   %s\n", mqttTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Sensor Task: %s\n", sensorTaskOk ? "OK" : "FAILED/SKIPPED");
    Serial.printf("  Actuator Tsk:%s\n", actuatorTasksOk ? "OK" : "FAILED/SKIPPED");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());

    if (displayOk) {
        // Mostrar um resumo do status ou limpar
        if (!sensorsOk || !actuatorsOk || !wifiOk || !mqttTaskOk || !timeOk) { // Adicionado !timeOk
             displayMgr.printLine(0, "System Started");
             displayMgr.printLine(1, "Degraded Mode"); // Indica que algo falhou ou está faltando
        } else {
            // A task do SensorManager começará a atualizar o display com dados
            // Poderia limpar aqui, mas a task sobrescreverá em breve.
             // displayMgr.clear(); // Opcional: Limpa mensagens de inicialização se tudo OK
        }
    }
    // O display agora será atualizado pelo SensorManager (se ok e a task rodar)
    // ou mostrará o erro/status final da inicialização.
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