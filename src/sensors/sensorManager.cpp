// src/sensors/sensorManager.cpp
// ******************************************
// *** RECONSTRUÍDO BASEADO NO .HPP E LÓGICA ANTERIOR ***
// ******************************************

#include "sensorManager.hpp"
#include "ui/displayManager.hpp" // Incluir para a dependência opcional
// #include "network/mqttManager.hpp" // Incluir se/quando for usar MqttManager
#include <Arduino.h>           // Para Serial, analogRead, vTaskDelay, etc.
#include <math.h>              // Para isnan, expf, abs
#include <ArduinoJson.h>       // Se for re-introduzir a publicação JSON

namespace GrowController {

// Definição das constantes estáticas declaradas no .hpp
const TickType_t SensorManager::SENSOR_READ_INTERVAL_MS = pdMS_TO_TICKS(3000); // Ex: 3 segundos
const TickType_t SensorManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(50);             // Ex: 50ms

// --- Construtor e Destrutor ---

SensorManager::SensorManager(const SensorConfig& config, DisplayManager* displayMgr, MqttManager* mqttMgr) :
    sensorConfig(config),         // Inicializa referência usando initializer list
    displayManager(displayMgr),   // Armazena ponteiro para DisplayManager (pode ser nullptr)
    mqttManager(mqttMgr),         // Armazena ponteiro para MqttManager (pode ser nullptr)
    dhtSensor(nullptr),           // unique_ptr inicializado como null
    // sensorDataMutex é inicializado automaticamente pelo seu construtor padrão
    cachedTemperature(NAN),       // Inicializa cache com NAN
    cachedHumidity(NAN),
    readTaskHandle(nullptr),      // Nenhum handle de tarefa ainda
    initialized(false)            // Manager não inicializado ainda
{
    // O corpo do construtor pode ficar vazio ou conter logs iniciais, se desejado.
    // Serial.println("SensorManager: Constructor called.");
}

SensorManager::~SensorManager() {
    Serial.println("SensorManager: Destructor called.");
    // Garante que a tarefa seja parada e deletada se estiver rodando
    if (readTaskHandle != nullptr) {
        Serial.println("SensorManager: Stopping sensor task...");
        vTaskDelete(readTaskHandle);
        readTaskHandle = nullptr; // Importante zerar o handle
    }
    // A limpeza dos recursos gerenciados por RAII (dhtSensor e sensorDataMutex)
    // acontece automaticamente aqui quando os membros saem de escopo.
    Serial.println("SensorManager: Destroyed.");
}

// --- Inicialização ---

bool SensorManager::initialize() {
     if (initialized) {
        Serial.println("SensorManager WARN: Already initialized.");
        return true;
    }
    Serial.println("SensorManager: Initializing...");

    // 1. Verifica se o Mutex foi criado com sucesso no construtor de FreeRTOSMutex
    if (!sensorDataMutex) { // Usa o operator bool() do wrapper
        Serial.println("SensorManager ERROR: Failed to create data mutex!");
        return false;
    }
    Serial.println("SensorManager: Data mutex verified.");

    // 2. Aloca e inicializa o sensor DHT usando unique_ptr
    // Usar reset() ou make_unique (se disponível). std::nothrow evita exceções na falha de alocação.
    dhtSensor.reset(new (std::nothrow) DHT(sensorConfig.dhtPin, sensorConfig.dhtType));
    if (!dhtSensor) {
        Serial.println("SensorManager ERROR: Failed to allocate DHT sensor object!");
        // Não precisamos deletar o mutex manualmente, RAII cuida disso.
        return false;
    }

    // 3. Chama begin() no objeto DHT gerenciado pelo unique_ptr
    dhtSensor->begin();
    Serial.println("SensorManager: DHT Sensor Initialized via begin().");

    // Marca como inicializado apenas se tudo deu certo
    initialized = true;
    Serial.println("SensorManager: Initialization successful.");
    return true;
}

bool SensorManager::isInitialized() const {
    return initialized;
}

// --- Leitura de Cache (Métodos Públicos Thread-Safe) ---

float SensorManager::getCurrentTemperature() const {
    float temp = NAN; // Valor padrão de retorno em caso de falha
    if (!initialized) return NAN; // Não tentar ler se não inicializado

    // Tenta obter o mutex para ler o valor cacheado
    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        temp = this->cachedTemperature; // Lê o valor do cache
        xSemaphoreGive(sensorDataMutex.get()); // Libera o mutex
    } else {
        Serial.println("SensorManager WARN: getCurrentTemperature - Mutex timeout or invalid.");
        // Retorna NAN se não conseguiu o mutex a tempo
    }
    return temp;
}

float SensorManager::getCurrentHumidity() const {
    float hum = NAN; // Valor padrão de retorno
     if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        hum = this->cachedHumidity; // Lê o valor do cache
        xSemaphoreGive(sensorDataMutex.get());
    } else {
         Serial.println("SensorManager WARN: getCurrentHumidity - Mutex timeout or invalid.");
    }
    return hum;
}

// --- Leituras Diretas e Cálculos (Métodos Públicos) ---

// A leitura do ADC é inerentemente thread-safe no ESP32 (desde que não se use ADC2 e WiFi ao mesmo tempo sem cuidado)
float SensorManager::readSoilHumidity() const {
     if (!initialized) return NAN; // Sensor de solo depende da config passada

    const int NUM_READINGS = 10;
    const float ADC_MAX = 4095.0f;
    const int MAX_READING_ATTEMPTS = NUM_READINGS * 2;
    const int MIN_VALID_VALUE = 1; // Exemplo, ajuste se necessário

    std::vector<int> validReadings;
    validReadings.reserve(NUM_READINGS);
    int attempts = 0;

    while (validReadings.size() < NUM_READINGS && attempts < MAX_READING_ATTEMPTS) {
        int analogValue = analogRead(this->sensorConfig.soilHumiditySensorPin);
        int reading = analogValue; // Assumindo leitura direta por agora (0 = seco, 4095 = molhado)
        // Se o seu sensor for invertido (0 = molhado, 4095 = seco):
        // int reading = ADC_MAX - analogValue;

        if (reading >= MIN_VALID_VALUE && reading <= ADC_MAX) {
            validReadings.push_back(reading);
        }
        attempts++;
        if (validReadings.size() < NUM_READINGS) {
             vTaskDelay(pdMS_TO_TICKS(5)); // Pequeno delay entre leituras
        }
    }

    if (!validReadings.empty()) {
        double sum = std::accumulate(validReadings.begin(), validReadings.end(), 0.0);
        float average = static_cast<float>(sum) / validReadings.size();

        // Fórmula de porcentagem (ajuste conforme necessário para seu sensor)
        // Exemplo: 0 = 100%, ADC_MAX = 0%
        float percentage = 100.0f * (ADC_MAX - average) / ADC_MAX;

        percentage = constrain(percentage, 0.0f, 100.0f);
        return percentage;
    } else {
        Serial.println("SensorManager WARN: No valid readings from Soil Sensor.");
        return NAN;
    }
}

float SensorManager::calculateVpd(float temp, float hum) const {
    // Validação de entrada
    if (isnan(temp) || isnan(hum) || hum < 0.0f || hum > 100.0f) {
        return NAN;
    }

    const float A = 17.67f;
    const float B = 243.5f;
    const float ES_FACTOR = 6.112f; // hPa

    float es = ES_FACTOR * expf((A * temp) / (temp + B));
    float ea = (hum / 100.0f) * es;
    float vpd_hpa = es - ea;
    if (vpd_hpa < 0.0f) { // VPD não pode ser negativo
        vpd_hpa = 0.0f;
    }
    float vpd_kpa = vpd_hpa / 10.0f;

    return vpd_kpa;
}


// --- Leitura Interna de Sensores DHT (Métodos Privados, Não Thread-Safe) ---

float SensorManager::_readTemperatureFromSensor() {
    if (!dhtSensor) { // Verifica se o unique_ptr contém um objeto
        // Serial.println("SensorManager DEBUG: DHT sensor object is null.");
        return NAN;
    }
    // Chama o método no objeto gerenciado pelo unique_ptr
    float temp = dhtSensor->readTemperature();
    // Serial.printf("SensorManager DEBUG: Read Temp Attempt: %.2f\n", temp); // Log de debug
    return temp;
}

float SensorManager::_readHumidityFromSensor() {
     if (!dhtSensor) {
        // Serial.println("SensorManager DEBUG: DHT sensor object is null.");
        return NAN;
    }
     float hum = dhtSensor->readHumidity();
    // Serial.printf("SensorManager DEBUG: Read Hum Attempt: %.2f\n", hum); // Log de debug
    return hum;
}

// --- Gerenciamento da Tarefa ---

bool SensorManager::startSensorTask(UBaseType_t priority, uint32_t stackSize) {
    if (!initialized) {
         Serial.println("SensorManager ERROR: Cannot start task, manager not initialized.");
         return false;
    }
    if (readTaskHandle != nullptr) {
        Serial.println("SensorManager WARN: Sensor task already started.");
        return true; // Tarefa já está rodando
    }

    Serial.println("SensorManager: Starting sensor reading task...");
    // Cria a tarefa, passando o wrapper estático e o ponteiro 'this'
    BaseType_t result = xTaskCreate(
        readSensorsTaskWrapper,   // A função estática que chamará o método da instância
        "SensorTask",             // Nome para debug
        stackSize,                // Tamanho da pilha em words (verificar necessidade)
        this,                     // Ponteiro para a instância atual de SensorManager
        priority,                 // Prioridade da tarefa
        &this->readTaskHandle     // Endereço para armazenar o handle da tarefa criada
    );

    if (result != pdPASS) {
        readTaskHandle = nullptr; // Garante que handle seja null se a criação falhar
        Serial.print("SensorManager ERROR: Failed to create sensor task! Error code: ");
        Serial.println(result);
        return false;
    }

    Serial.println("SensorManager: Sensor task started successfully.");
    return true;
}

// --- Implementação da Tarefa (Wrapper Estático e Loop Principal) ---

// Esta função estática é o ponto de entrada para a tarefa FreeRTOS.
// Ela recebe o ponteiro 'this' e chama o método de loop da instância correta.
void SensorManager::readSensorsTaskWrapper(void *pvParameters) {
    Serial.println("SensorTaskWrapper: Task initiated.");
    // Converte o parâmetro void* de volta para um ponteiro da classe
    SensorManager* instance = static_cast<SensorManager*>(pvParameters);

    if (instance != nullptr) {
        Serial.println("SensorTaskWrapper: Instance valid, entering runSensorTask loop.");
        // Chama o método da instância que contém o loop infinito
        instance->runSensorTask();
    } else {
        Serial.println("SensorTaskWrapper ERROR: Received null instance pointer! Cannot run task.");
    }

    // Esta linha só será alcançada se runSensorTask() retornar (o que não deve
    // acontecer em um loop while(true)), ou se a instância for nula.
    Serial.println("SensorTaskWrapper: Task finishing (should not happen in normal operation). Deleting task.");
    vTaskDelete(NULL); // Auto-deleta a tarefa
}

// Este método contém o loop principal que será executado pela tarefa.
void SensorManager::runSensorTask() {
    Serial.println("SensorManager: runSensorTask loop entered.");

    // Alocar JSON doc aqui se a publicação JSON for reativada
    // StaticJsonDocument<256> sensorDoc;

    while (true) {
        if (!initialized) { // Checagem extra dentro do loop
             Serial.println("SensorTask ERROR: Manager is no longer initialized. Stopping task.");
             break; // Sai do loop while(true)
        }

        // --- 1. Ler Sensores (usando métodos privados) ---
        float currentTemperature = _readTemperatureFromSensor();
        float currentAirHumidity = _readHumidityFromSensor();
        // Leitura do solo pode ser feita aqui ou via getter público se preferir
        float currentSoilHumidity = readSoilHumidity(); // Leitura direta no momento

        // --- 2. Atualizar Cache (acesso thread-safe) ---
        if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
            // Atualiza o cache apenas se a leitura DHT foi válida
            if (!isnan(currentTemperature)) {
                this->cachedTemperature = currentTemperature;
            }
             if (!isnan(currentAirHumidity)) {
                this->cachedHumidity = currentAirHumidity;
            }
            // Cache do solo não é mantido nesta implementação, leitura é direta.
            xSemaphoreGive(sensorDataMutex.get());
        } else {
             Serial.println("SensorTask WARN: Failed acquire mutex to update cache.");
             // Continuar mesmo assim? Sim, provavelmente.
        }

        // --- 3. Calcular VPD (com leituras atuais) ---
        float currentVpd = calculateVpd(currentTemperature, currentAirHumidity);

        // --- 4. Processar/Publicar ---
        bool validReadings = !isnan(currentTemperature) &&
                             !isnan(currentAirHumidity) &&
                             !isnan(currentSoilHumidity) && // Soil é obrigatório? Decida.
                             !isnan(currentVpd);

        if (validReadings) {
            // Log opcional das leituras válidas
            // Serial.printf("SensorTask: T=%.1fC, H=%.0f%%, S=%.0f%%, VPD=%.2fkPa\n",
            //        currentTemperature, currentAirHumidity, currentSoilHumidity, currentVpd);

            // Publicação MQTT (se MqttManager for implementado e injetado)
            if (mqttManager != nullptr /* && mqttManager->isConnected() */ ) { // Verificar se conectado
                 // Exemplo: Publicar valores individuais
                 // mqttManager->publish("sensors/temperature", currentTemperature);
                 // mqttManager->publish("sensors/air_humidity", currentAirHumidity);
                 // mqttManager->publish("sensors/soil_humidity", currentSoilHumidity);
                 // mqttManager->publish("sensors/vpd", currentVpd);

                 // Exemplo: Publicar JSON
                 /*
                 sensorDoc.clear();
                 sensorDoc["temperature"] = serializedValue(currentTemperature, 1); // Formata com 1 decimal
                 sensorDoc["airHumidity"] = serializedValue(currentAirHumidity, 0); // 0 decimais
                 sensorDoc["soilHumidity"] = serializedValue(currentSoilHumidity, 0);
                 sensorDoc["vpd"] = serializedValue(currentVpd, 2); // 2 decimais
                 String payload;
                 serializeJson(sensorDoc, payload);
                 mqttManager->publish("sensors/all", payload.c_str());
                 */
            }

        } else {
            Serial.println("SensorTask WARN: Invalid sensor readings obtained in this cycle.");
            // Não publicar ou atualizar display com dados inválidos?
        }

        // --- 5. Atualizar Display (se DisplayManager for injetado) ---
        // Passa as leituras *deste ciclo*, válidas ou não (o display tratará NAN)
        if (displayManager != nullptr && displayManager->isInitialized()) {
            displayManager->showSensorData(currentTemperature, currentAirHumidity, currentSoilHumidity);
        }

        // --- 6. Aguardar próximo ciclo ---
        vTaskDelay(SENSOR_READ_INTERVAL_MS);
    }
     // Fim do while(true) - esta parte não deve ser alcançada normalmente
}


} // namespace GrowController