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
SemaphoreHandle_t mqttMutex = xSemaphoreCreateMutex(); // OK, será gerenciado pelo MqttManager no futuro
// LiquidCrystal_I2C LCD(0x27, 16, 2); // REMOVIDO - Agora dentro do DisplayManager
// SemaphoreHandle_t lcdMutex = xSemaphoreCreateMutex(); // REMOVIDO - Agora dentro do DisplayManager

AppConfig appConfig; // Configuração global (ainda precisa ser injetada)
// TargetValues target; // Definido em mqttHandler.cpp (ainda global)
// -----------------------------------------------------------

void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    // 1. Inicializa módulos independentes
    initializeSensors();
    initializeActuators(appConfig.gpioControl); // Passa a parte relevante da config

    // 2. Inicializa comunicação I2C e o Display Manager
    Wire.begin(SDA, SCL); // Usa pinos definidos em config.hpp -> board_xxx.hpp
    if (!displayManagerInit(0x27, 16, 2)) { // <--- Inicializa Display Manager
        Serial.println("FATAL ERROR: Display Manager Initialization Failed!");
        // O que fazer aqui? Parar? Reiniciar? Por enquanto, continua.
        // Pode ser útil ter um LED de erro piscando.
    }
    displayManagerShowBooting(); // <--- Usa Display Manager

    // 3. Inicia a Tarefa de Conexão WiFi
    Serial.println("Starting WiFi Task...");
    // Mensagem no display será mostrada pela própria tarefa WiFi agora
    xTaskCreate(
        connectToWiFi,      // Função da tarefa (definida em wifi.cpp)
        "WiFiTask",         // Nome da tarefa
        4096,               // Tamanho da pilha
        NULL,               // Parâmetros da tarefa
        1,                  // Prioridade da tarefa
        NULL                // Handle da tarefa (opcional)
    );

    // 4. Aguarda Conexão WiFi (Ainda bloqueante, mas necessário para NTP/MQTT)
    Serial.print("Waiting for WiFi connection...");
    // A tarefa WiFi deve atualizar o display com o status/spinner
    while (WiFi.status() != WL_CONNECTED) {
        // O spinner é atualizado dentro da tarefa connectToWiFi via displayManagerUpdateSpinner()
        vTaskDelay(pdMS_TO_TICKS(500)); // Pausa não bloqueante
    }
    Serial.println("\nWiFi Connected!");
    // A tarefa connectToWiFi já deve ter mostrado o IP no display

    // 5. Inicializa o Serviço de Tempo (depende de WiFi)
    Serial.println("Initializing Time Service...");
    displayManagerShowNtpSyncing(); // <--- Usa Display Manager
    if (!initializeTimeService(appConfig.time)) {
        Serial.println("ERROR: Failed to initialize Time Service!");
        displayManagerShowError("NTP Fail"); // <--- Usa Display Manager
    } else {
        displayManagerShowNtpSynced(); // <--- Usa Display Manager
        // Pequena pausa opcional para ver a mensagem NTP OK
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 6. Inicia Tarefa MQTT (depende de WiFi)
    Serial.println("Starting MQTT Task...");
    displayManagerShowMqttConnecting(); // <--- Usa Display Manager (a tarefa pode atualizar para conectado)
    xTaskCreate(
        manageMQTT,         // Função da tarefa (definida em mqtt.cpp)
        "MQTTTask",
        4096,
        NULL,
        2,                  // Prioridade
        NULL
    );

    // 7. Inicia Tarefa de Leitura de Sensores
    Serial.println("Starting Sensor Reading Task...");
    // A tarefa readSensors agora atualiza o display com os dados
    xTaskCreate(
        readSensors,        // Função da tarefa (definida em sensorManager.cpp)
        "SensorTask",
        4096,
        NULL,
        1,                  // Prioridade
        NULL
    );

    // 8. Inicia Tarefa de Controle de Atuadores (Luz)
    Serial.println("Starting Light Control Task...");
    // Nenhuma mensagem específica no display para esta tarefa por enquanto
    xTaskCreate(
        lightControlTask,   // Função da tarefa (definida em actuatorManager.cpp)
        "LightControlTask",
        2048,
        NULL,
        1,                  // Prioridade
        NULL
    );

    // Pequena pausa antes de limpar a tela e deixar as tarefas rodarem
    vTaskDelay(pdMS_TO_TICKS(500));
    // displayManagerClear(); // Opcional: Limpar a tela após o setup ou deixar a SensorTask assumir

    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
    // O loop principal fica vazio, pois tudo é tratado pelas tarefas FreeRTOS.
    vTaskDelay(pdMS_TO_TICKS(1000));
}