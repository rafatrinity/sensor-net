// main.ino

#include "config.hpp"          // Configurações gerais e de placa
#include "network/wifi.hpp"    // Para connectToWiFi task
#include "network/mqtt.hpp"    // Para manageMQTT task
#include "sensors/sensorManager.hpp" // Para initializeSensors e readSensors task
#include "actuatorManager.hpp" // Para initializeActuators e lightControlTask task
#include "sensors/targets.hpp" // Para a struct TargetValues (ainda global)
#include "utils/timeService.hpp"   // Para initializeTimeService
#include "ui/displayManager.hpp" // Para todas as interações com o display

#include <Wire.h>          // Necessário para I2C
#include <ArduinoJson.h>   // Para o JsonDocument global 'doc'

// --- Variáveis Globais Remanescentes ---
JsonDocument doc; // Usado na tarefa readSensors (ainda global)
SemaphoreHandle_t mqttMutex = xSemaphoreCreateMutex(); 
TargetValues target;
// TargetValues target; // Definido em mqttHandler.cpp (ainda global)

// --- Instância Principal da Configuração ---
// Agora é uma variável com escopo global *apenas* dentro deste arquivo .ino
// As tarefas e funções receberão ponteiros/referências para ela ou suas partes.
AppConfig appConfig;
// -----------------------------------------------------------

void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    // 1. Inicializa módulos independentes
    // Passa a parte relevante da config por referência constante
    initializeSensors(appConfig.sensor);         // <<< MODIFICADO
    initializeActuators(appConfig.gpioControl); // Já estava correto

    // 2. Inicializa comunicação I2C e o Display Manager
    Wire.begin(SDA, SCL);
    if (!displayManagerInit(0x27, 16, 2)) {
        Serial.println("FATAL ERROR: Display Manager Initialization Failed!");
    }
    displayManagerShowBooting();

    // 3. Inicia a Tarefa de Conexão WiFi
    Serial.println("Starting WiFi Task...");
    xTaskCreate(
        connectToWiFi,      // Função da tarefa
        "WiFiTask",         // Nome
        4096,               // Pilha
        (void*)&appConfig.wifi, // <<< MODIFICADO: Passa ponteiro para WiFiConfig
        1,                  // Prioridade
        NULL                // Handle
    );

    // 4. Aguarda Conexão WiFi
    Serial.print("Waiting for WiFi connection...");
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.println("\nWiFi Connected!");

    // 5. Inicializa o Serviço de Tempo (depende de WiFi)
    Serial.println("Initializing Time Service...");
    displayManagerShowNtpSyncing();
    // Passa a parte relevante da config por referência constante
    if (!initializeTimeService(appConfig.time)) { // Já estava correto
        Serial.println("ERROR: Failed to initialize Time Service!");
        displayManagerShowError("NTP Fail");
    } else {
        displayManagerShowNtpSynced();
    }

    // 6. Inicia Tarefa MQTT (depende de WiFi)
    Serial.println("Starting MQTT Task...");
    displayManagerShowMqttConnecting();
    xTaskCreate(
        manageMQTT,         // Função da tarefa
        "MQTTTask",         // Nome
        4096,               // Pilha
        (void*)&appConfig.mqtt, // <<< MODIFICADO: Passa ponteiro para MQTTConfig
        2,                  // Prioridade
        NULL                // Handle
    );

    // 7. Inicia Tarefa de Leitura de Sensores
    Serial.println("Starting Sensor Reading Task...");
    xTaskCreate(
        readSensors,        // Função da tarefa
        "SensorTask",       // Nome
        4096,               // Pilha
        (void*)&appConfig,  // <<< MODIFICADO: Passa ponteiro para AppConfig completo
        1,                  // Prioridade
        NULL                // Handle
    );

    // 8. Inicia Tarefa de Controle de Atuadores (Luz)
    Serial.println("Starting Light Control Task...");
    xTaskCreate(
        lightControlTask,   // Função da tarefa
        "LightControlTask", // Nome
        2048,               // Pilha
        (void*)&appConfig.gpioControl, // <<< MODIFICADO: Passa ponteiro para GPIOControlConfig
        1,                  // Prioridade
        NULL                // Handle
    );

    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}