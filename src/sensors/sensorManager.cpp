// src/sensors/sensorManager.cpp
#include "sensorManager.hpp"
#include "ui/displayManager.hpp"      // Para interação com o display
#include "network/mqttManager.hpp"  // Para publicação MQTT
#include <Arduino.h>                // Funções Core Arduino/ESP-IDF
#include <math.h>                   // Para isnan, expf, abs, round
#include <vector>                   // Para leitura do sensor de solo
#include <numeric>                  // Para std::accumulate
#include <ArduinoJson.h>            // Para publicação JSON (opcional)

namespace GrowController {

// Definição das constantes estáticas declaradas no .hpp
const TickType_t SensorManager::SENSOR_READ_INTERVAL_MS = pdMS_TO_TICKS(3000);
const TickType_t SensorManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(50);

// --- Construtor e Destrutor ---

SensorManager::SensorManager(const SensorConfig& config, DisplayManager* displayMgr, MqttManager* mqttMgr) :
    sensorConfig(config),
    displayManager(displayMgr),
    mqttManager(mqttMgr),
    dhtSensor(nullptr),
    cachedTemperature(NAN),
    cachedHumidity(NAN),
    readTaskHandle(nullptr),
    initialized(false)
{}

SensorManager::~SensorManager() {
    // Garante que a tarefa seja parada e deletada se estiver rodando
    if (readTaskHandle != nullptr) {
        Serial.println("SensorManager: Stopping sensor task...");
        vTaskDelete(readTaskHandle);
        readTaskHandle = nullptr;
    }
    // Limpeza de dhtSensor (unique_ptr) e sensorDataMutex (FreeRTOSMutex) é automática (RAII)
    Serial.println("SensorManager: Destroyed.");
}

// --- Inicialização ---

bool SensorManager::initialize() {
     if (initialized) {
        Serial.println("SensorManager WARN: Already initialized.");
        return true;
    }
    Serial.println("SensorManager: Initializing...");

    if (!sensorDataMutex) {
        Serial.println("SensorManager ERROR: Failed to create data mutex!");
        return false;
    }
    Serial.println("SensorManager: Data mutex verified.");

    dhtSensor.reset(new (std::nothrow) DHT(sensorConfig.dhtPin, sensorConfig.dhtType));
    if (!dhtSensor) {
        Serial.println("SensorManager ERROR: Failed to allocate DHT sensor object!");
        return false;
    }

    dhtSensor->begin();
    Serial.println("SensorManager: DHT Sensor Initialized via begin().");

    initialized = true;
    Serial.println("SensorManager: Initialization successful.");
    return true;
}

bool SensorManager::isInitialized() const {
    return initialized;
}

// --- Leitura de Cache (Métodos Públicos Thread-Safe) ---

float SensorManager::getCurrentTemperature() const {
    float temp = NAN;
    if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        temp = this->cachedTemperature;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        // Evitar log excessivo em caso de timeout frequente
        // Serial.println("SensorManager WARN: getCurrentTemperature - Mutex timeout or invalid.");
    }
    return temp;
}

float SensorManager::getCurrentHumidity() const {
    float hum = NAN;
     if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        hum = this->cachedHumidity;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        // Serial.println("SensorManager WARN: getCurrentHumidity - Mutex timeout or invalid.");
    }
    return hum;
}

// --- Leituras Diretas e Cálculos (Métodos Públicos) ---

float SensorManager::readSoilHumidity() const {
     if (!initialized) return NAN;

    const int NUM_READINGS = 10;
    const float ADC_MAX = 4095.0f;
    const int MAX_READING_ATTEMPTS = NUM_READINGS * 2; // Limite para evitar bloqueio
    const int MIN_VALID_VALUE = 1; // Valor mínimo considerado válido

    std::vector<int> validReadings;
    validReadings.reserve(NUM_READINGS);
    int attempts = 0;

    while (validReadings.size() < NUM_READINGS && attempts < MAX_READING_ATTEMPTS) {
        int analogValue = analogRead(this->sensorConfig.soilHumiditySensorPin);
        // Assumindo leitura direta (0 = seco, 4095 = molhado) - Inverta se necessário
        int reading = analogValue;
        // Exemplo de inversão: int reading = ADC_MAX - analogValue;

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

        // Fórmula de porcentagem - AJUSTE CONFORME SEU SENSOR E CALIBRAÇÃO
        // Exemplo 1: Mapeamento linear invertido (ADC_MAX = 0%, 0 = 100%)
        float percentage = 100.0f * (ADC_MAX - average) / ADC_MAX;
        // Exemplo 2: Mapeamento linear direto (0 = 0%, ADC_MAX = 100%)
        // float percentage = 100.0f * average / ADC_MAX;

        percentage = constrain(percentage, 0.0f, 100.0f); // Garante faixa 0-100%
        return percentage;
    } else {
        Serial.println("SensorManager WARN: No valid readings from Soil Sensor.");
        return NAN;
    }
}

float SensorManager::calculateVpd(float temp, float hum) const {
    if (isnan(temp) || isnan(hum) || hum < 0.0f || hum > 100.0f) {
        return NAN;
    }

    const float A = 17.67f;
    const float B = 243.5f;
    const float ES_FACTOR = 6.112f; // hPa

    float es = ES_FACTOR * expf((A * temp) / (temp + B)); // Pressão de vapor de saturação
    float ea = (hum / 100.0f) * es; // Pressão de vapor atual
    float vpd_hpa = es - ea;
    if (vpd_hpa < 0.0f) {
        vpd_hpa = 0.0f; // VPD não pode ser negativo
    }
    float vpd_kpa = vpd_hpa / 10.0f; // Converte para kPa

    return vpd_kpa;
}


// --- Leitura Interna de Sensores DHT (Métodos Privados, Não Thread-Safe) ---

float SensorManager::_readTemperatureFromSensor() {
    if (!dhtSensor) { return NAN; }
    return dhtSensor->readTemperature();
}

float SensorManager::_readHumidityFromSensor() {
     if (!dhtSensor) { return NAN; }
     return dhtSensor->readHumidity();
}

// --- Gerenciamento da Tarefa ---

bool SensorManager::startSensorTask(UBaseType_t priority, uint32_t stackSize) {
    if (!initialized) {
         Serial.println("SensorManager ERROR: Cannot start task, manager not initialized.");
         return false;
    }
    if (readTaskHandle != nullptr) {
        Serial.println("SensorManager WARN: Sensor task already started.");
        return true;
    }

    Serial.println("SensorManager: Starting sensor reading task...");
    BaseType_t result = xTaskCreate(
        readSensorsTaskWrapper,
        "SensorTask",
        stackSize,
        this, // Passa ponteiro da instância atual como parâmetro
        priority,
        &this->readTaskHandle
    );

    if (result != pdPASS) {
        readTaskHandle = nullptr;
        Serial.print("SensorManager ERROR: Failed to create sensor task! Error code: ");
        Serial.println(result);
        return false;
    }

    Serial.println("SensorManager: Sensor task started successfully.");
    return true;
}

// --- Implementação da Tarefa (Wrapper Estático e Loop Principal) ---

void SensorManager::readSensorsTaskWrapper(void *pvParameters) {
    // Serial.println("SensorTaskWrapper: Task initiated."); // Log de baixo nível
    SensorManager* instance = static_cast<SensorManager*>(pvParameters);
    if (instance != nullptr) {
        // Serial.println("SensorTaskWrapper: Instance valid, entering runSensorTask loop.");
        instance->runSensorTask(); // Chama o loop da instância
    } else {
        Serial.println("SensorTaskWrapper ERROR: Received null instance pointer! Cannot run task.");
    }
    Serial.println("SensorTaskWrapper: Task finishing. Deleting task.");
    vTaskDelete(NULL); // Auto-deleta a tarefa ao sair (embora não deva sair)
}

void SensorManager::runSensorTask() {
    // Serial.println("SensorManager: runSensorTask loop entered.");

    // Opcional: Alocar JSON doc uma vez se for usar sempre
    JsonDocument sensorDoc;

    while (true) {
        if (!initialized) {
             Serial.println("SensorTask ERROR: Manager is no longer initialized. Stopping task.");
             break; // Sai do loop while(true), tarefa será deletada
        }

        // --- 1. Ler Sensores ---
        float currentTemperature = _readTemperatureFromSensor();
        float currentAirHumidity = _readHumidityFromSensor();
        float currentSoilHumidity = readSoilHumidity();

        // --- 2. Atualizar Cache ---
        if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
            if (!isnan(currentTemperature)) { this->cachedTemperature = currentTemperature; }
            if (!isnan(currentAirHumidity)) { this->cachedHumidity = currentAirHumidity; }
            xSemaphoreGive(sensorDataMutex.get());
        } else {
             Serial.println("SensorTask WARN: Failed acquire mutex to update cache.");
        }

        // --- 3. Calcular VPD ---
        float currentVpd = calculateVpd(currentTemperature, currentAirHumidity);

        // --- 4. Processar/Publicar ---
        bool validReadings = !isnan(currentTemperature) &&
                             !isnan(currentAirHumidity) &&
                             !isnan(currentSoilHumidity) && // Considerar se soil é opcional
                             !isnan(currentVpd);

        if (validReadings) {
            // Publicação MQTT via MqttManager injetado
            if (this->mqttManager != nullptr ) {
                // Publicar valores individuais
                this->mqttManager->publish("sensors/temperature", currentTemperature);
                this->mqttManager->publish("sensors/air_humidity", currentAirHumidity);
                this->mqttManager->publish("sensors/soil_humidity", currentSoilHumidity);
                this->mqttManager->publish("sensors/vpd", currentVpd);

                // Ou publicar um único JSON
                /*
                sensorDoc.clear();
                // Arredonda para consistência e tamanho da mensagem
                sensorDoc["temperature"] = round(currentTemperature * 10.0) / 10.0; // 1 decimal
                sensorDoc["airHumidity"] = round(currentAirHumidity);             // Inteiro
                sensorDoc["soilHumidity"] = round(currentSoilHumidity);            // Inteiro
                sensorDoc["vpd"] = round(currentVpd * 100.0) / 100.0;              // 2 decimais
                String payload;
                serializeJson(sensorDoc, payload);
                this->mqttManager->publish("sensors/state", payload.c_str());
                */
            }
        } else {
            Serial.println("SensorTask WARN: Invalid sensor readings obtained in this cycle.");
        }

        // --- 5. Atualizar Display via DisplayManager injetado ---
        if (this->displayManager != nullptr && this->displayManager->isInitialized()) {
            // Passa as leituras do ciclo atual (display trata NAN)
            this->displayManager->showSensorData(currentTemperature, currentAirHumidity, currentSoilHumidity);
        }

        // --- 6. Aguardar próximo ciclo ---
        vTaskDelay(SENSOR_READ_INTERVAL_MS);
    }
}

} // namespace GrowController