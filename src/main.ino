#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqtt.hpp"
#include "sensors/sensorManager.hpp"
#include "actuatorManager.hpp"
#include "sensors/targets.hpp"
#include "utils/timeService.hpp"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// --- Variáveis Globais (Candidatas à Refatoração/Injeção) ---
LiquidCrystal_I2C LCD(0x27, 16, 2); // Endereço I2C pode variar, verificar com I2C Scanner
JsonDocument doc;                   // Usado na tarefa readSensors (globalmente)
SemaphoreHandle_t mqttMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t lcdMutex = xSemaphoreCreateMutex();
//TODO: SemaphoreHandle_t targetMutex = xSemaphoreCreateMutex(); // Para proteger acesso à struct target
//TODO: SemaphoreHandle_t ntpMutex = xSemaphoreCreateMutex(); // Se TimeService precisar de proteção

AppConfig appConfig; // Configuração global
// TargetValues target; // A instância 'target' é definida em mqttHandler.cpp (global)
// -----------------------------------------------------------

void setup() {
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    initializeSensors();

    initializeActuators(appConfig.gpioControl);

    Wire.begin(SDA, SCL);
    LCD.init();
    LCD.backlight();
    LCD.clear();
    LCD.print("Booting...");

    Serial.println("Starting WiFi Task...");
    // Cria Tarefa para Conexão WiFi
    xTaskCreate(
        connectToWiFi,       // Função da tarefa (definida em wifi.cpp)
        "WiFiTask",          // Nome da tarefa
        4096,                // Tamanho da pilha
        NULL,                // Parâmetros da tarefa
        1,                   // Prioridade da tarefa
        NULL                 // Handle da tarefa (opcional)
    );

    // Aguarda Conexão WiFi (Bloqueante - não ideal para inicializações paralelas)
    Serial.print("Waiting for WiFi connection...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(500)); // Pausa não bloqueante (permite outras tarefas rodarem se já existissem)
    }
    Serial.println("\nWiFi Connected!");
    LCD.setCursor(0,1);
    LCD.print("WiFi OK");
    delay(500); // Pequena pausa para ver a mensagem

    Serial.println("Initializing NTP...");
    LCD.setCursor(0,0);
    LCD.print("Syncing Time...");
    initializeTimeService(appConfig.time);
    LCD.setCursor(0,1);
    LCD.print("NTP OK    ");
    delay(500);


    Serial.println("Starting MQTT Task...");
    LCD.clear();
    LCD.print("Starting MQTT...");
    // Cria Tarefa para Gerenciamento MQTT (depende de WiFi)
    xTaskCreate(
        manageMQTT,         // Função da tarefa (definida em mqtt.cpp)
        "MQTTTask",
        4096,
        NULL,
        2,                  // Prioridade ligeiramente maior pode ser útil para manter conexão
        NULL
    );

    Serial.println("Starting Sensor Reading Task...");
    LCD.setCursor(0,1);
    LCD.print("Sensors Task...");
    // Cria Tarefa para Leitura de Sensores
    xTaskCreate(
        readSensors,        // Função da tarefa (definida em sensorManager.cpp)
        "SensorTask",
        4096,               // Pode precisar de mais pilha se fizer muita coisa com JSON/String
        NULL,
        1,                  // Prioridade normal
        NULL
    );

    Serial.println("Starting Light Control Task...");
    LCD.setCursor(0,0);
    LCD.print("Control Task...");
     // Cria Tarefa para Controle de Luz (depende de NTP)
    xTaskCreate(
        lightControlTask,   // Função da tarefa (definida em actuatorManager.cpp)
        "LightControlTask",
        2048,               // Pilha menor pode ser suficiente para esta tarefa simples
        NULL,
        1,                  // Prioridade normal
        NULL
    );
    delay(500);
    LCD.clear();

    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: "); Serial.println(ESP.getFreeHeap());
}

void loop() {
    // O loop principal fica vazio, pois tudo é tratado pelas tarefas FreeRTOS.
    vTaskDelay(pdMS_TO_TICKS(1000)); // Pequeno delay para evitar que o loop rode muito rápido (embora não faça nada)
                                     // E permite que a tarefa IDLE (prioridade 0) rode para limpar recursos.
}