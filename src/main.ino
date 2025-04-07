#include "config.hpp"
#include "network/wifi.hpp"
#include "network/mqtt.hpp"          // Ainda necessário para a tarefa antiga
#include "sensors/sensorManager.hpp"
#include "actuators/actuatorManager.hpp"
#include "data/targetDataManager.hpp" 
#include "utils/timeService.hpp"
#include "ui/displayManager.hpp"

#include <Wire.h>
#include <ArduinoJson.h> // Ainda necessário para readSensors localmente

// --- Instâncias Principais ---
AppConfig appConfig;
GrowController::TargetDataManager targetManager; // <<< INSTANCIAR O MANAGER

// Estrutura para passar parâmetros para as tarefas de atuadores <<< ADICIONADO
struct ActuatorTaskParams {
    const GPIOControlConfig* gpioConfig;
    GrowController::TargetDataManager* targetManager;
};
// -----------------------------------------------------------

void setup()
{
    Serial.begin(BAUD);
    Serial.println("\n--- Booting Application ---");

    // 1. Inicializa módulos independentes
    initializeSensors(appConfig.sensor);
    // Mover initializeActuators para src/actuators se ainda não o fez
    initializeActuators(appConfig.gpioControl);

    // 2. Inicializa I2C e Display
    Wire.begin(SDA, SCL);
    if (!displayManagerInit(0x27, 16, 2)) { // Endereço e tamanho como exemplo
        Serial.println("FATAL ERROR: Display Manager Initialization Failed!");
        // Lidar com erro? Parar?
    }
    displayManagerShowBooting();

    // 3. Inicia Tarefa WiFi
    Serial.println("Starting WiFi Task...");
    xTaskCreate(
        connectToWiFi,
        "WiFiTask",
        4096,
        (void *)&appConfig.wifi,
        1,
        NULL);

    // 4. Aguarda WiFi
    Serial.print("Waiting for WiFi connection...");
    displayManagerShowConnectingWiFi(); // Mostrar antes do loop
    while (WiFi.status() != WL_CONNECTED) {
         displayManagerUpdateSpinner(); // Atualizar spinner enquanto espera
        vTaskDelay(pdMS_TO_TICKS(100)); // Espera mais curta para spinner mais fluido
    }
    Serial.println("\nWiFi Connected!");
    // Display já é atualizado por connectToWiFi ao conectar

    // 5. Inicializa Time Service
    Serial.println("Initializing Time Service...");
    displayManagerShowNtpSyncing();
    if (!initializeTimeService(appConfig.time)) {
        Serial.println("ERROR: Failed to initialize Time Service!");
        displayManagerShowError("NTP Fail");
    } else {
        // Display já é atualizado por initializeTimeService se for bem-sucedido
        displayManagerShowNtpSynced(); // Ou pode ser redundante
    }

    // 6. Inicia Tarefa MQTT (Ainda usando a versão antiga por enquanto)
    Serial.println("Starting MQTT Task...");
    displayManagerShowMqttConnecting();
    xTaskCreate(
        manageMQTT,             // Função antiga
        "MQTTTask",
        4096,
        (void *)&appConfig.mqtt, // Passa config antiga
        2,
        NULL);

    // 7. Inicia Tarefa de Leitura de Sensores
    //    Esta tarefa agora precisa de acesso ao MqttManager e DisplayManager.
    //    Temporariamente, vamos remover a parte de publicação MQTT até MqttManager estar pronto.
    Serial.println("Starting Sensor Reading Task...");
    xTaskCreate(
        readSensors,        // Função da tarefa
        "SensorTask",
        4096,               // Pilha pode precisar de ajuste
        (void *)&appConfig, // Passa AppConfig (ainda usa para ler config de pinos)
                            // Futuramente passará ponteiros para MqttManager, DisplayManager
        1,
        NULL);


    // --- Preparar parâmetros para tarefas de atuadores --- <<< ADICIONADO
    ActuatorTaskParams actuatorParams = {
        &appConfig.gpioControl,
        &targetManager // Passa o ponteiro para o manager instanciado
    };


    // 8. Inicia Tarefa de Controle de Luz
    Serial.println("Starting Light Control Task...");
    xTaskCreate(
        lightControlTask,
        "LightControlTask",
        2048,
        (void *)&actuatorParams, // <<< Passa a struct de parâmetros
        1,
        NULL);

    // 9. Inicia Tarefa de Controle de Umidade
    Serial.println("Starting Humidity Control Task...");
    xTaskCreate(
        humidityControlTask,
        "HumidityCtrlTask",
        2560,
        (void *)&actuatorParams, // <<< Passa a struct de parâmetros
        1,
        NULL);

    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("--- Setup Complete ---");
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
}

void loop()
{
    // O loop principal pode ficar vazio ou fazer tarefas de baixa prioridade/verificações
    vTaskDelay(portMAX_DELAY); // Dorme indefinidamente se não houver nada a fazer
}