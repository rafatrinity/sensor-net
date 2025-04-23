// src/sensors/sensorManager.cpp
#include "sensorManager.hpp"
#include "ui/displayManager.hpp"      // Para interação com o display
#include "network/mqttManager.hpp"  // Para publicação MQTT
#include <Arduino.h>                // Funções Core Arduino/ESP-IDF
#include <math.h>                   // Para isnan, expf, abs, round
#include <vector>                   // Para leitura do sensor de solo
#include <numeric>                  // Para std::accumulate
#include <ArduinoJson.h>            // Para publicação JSON (opcional)
#include "config.hpp"

namespace GrowController {

// Definição das constantes estáticas declaradas no .hpp
const TickType_t SensorManager::SENSOR_READ_INTERVAL_MS = pdMS_TO_TICKS(3000);
const TickType_t SensorManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(500);

// --- Construtor e Destrutor ---

SensorManager::SensorManager(const SensorConfig& config, DisplayManager* displayMgr, MqttManager* mqttMgr) :
    sensorConfig(config),
    displayManager(displayMgr),
    mqttManager(mqttMgr),
    dhtSensor(nullptr),
    cachedTemperature(NAN),
    cachedHumidity(NAN),
    cachedSoilHumidity(NAN),
    cachedVpd(NAN),          // Inicializa novo cache
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
       Serial.println("SensorManager ERROR: Failed to create data mutex! Cannot initialize.");
       return false; // Cannot proceed without mutex
   }
   Serial.println("SensorManager: Data mutex verified.");

   // Retry Loop for DHT Sensor Initialization
   for (int attempt = 1; attempt <= INIT_RETRY_COUNT; ++attempt) {
       Serial.printf("SensorManager: Attempt %d/%d to initialize DHT sensor...\n", attempt, INIT_RETRY_COUNT);

       dhtSensor.reset(new (std::nothrow) DHT(sensorConfig.dhtPin, sensorConfig.dhtType));
       if (!dhtSensor) {
           Serial.println("SensorManager ERROR: Failed to allocate DHT sensor object!");
            if (attempt < INIT_RETRY_COUNT) {
                vTaskDelay(pdMS_TO_TICKS(INIT_RETRY_DELAY_MS));
            }
           continue; // Try next attempt
       }

       dhtSensor->begin(); // begin() não retorna status, mas pode demorar um pouco

       // Tentativa de leitura inicial para verificar se o sensor responde
       float initialTemp = dhtSensor->readTemperature();
       float initialHum = dhtSensor->readHumidity();

       if (isnan(initialTemp) || isnan(initialHum)) {
           Serial.printf("SensorManager WARN: DHT sensor did not return valid data on attempt %d.\n", attempt);
           dhtSensor.reset(); // Libera o objeto falho
           if (attempt < INIT_RETRY_COUNT) {
               vTaskDelay(pdMS_TO_TICKS(INIT_RETRY_DELAY_MS));
           }
           // Continue para a próxima tentativa
       } else {
           Serial.printf("SensorManager: DHT Sensor Initialized successfully (Initial read: %.1fC, %.1f%%).\n", initialTemp, initialHum);
           initialized = true;
           Serial.println("SensorManager: Initialization successful.");
           return true; // Sai do loop e da função com sucesso
       }
   } // End of retry loop

   // Se o loop terminou sem sucesso
   Serial.println("SensorManager ERROR: Initialization failed after all retries.");
   dhtSensor.reset(); // Garante que o ponteiro é nulo
   initialized = false;
   return false;
}

bool SensorManager::isInitialized() const {
    return initialized;
}

// --- Leitura de Cache (Métodos Públicos Thread-Safe) ---

float SensorManager::getTemperature() const {
    float temp = NAN;
    if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        temp = this->cachedTemperature;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        // Evitar log excessivo em caso de timeout frequente
        Serial.println("SensorManager WARN: getTemperature - Mutex timeout or invalid.");
    }
    return temp;
}

float SensorManager::getHumidity() const {
    float hum = NAN;
     if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        hum = this->cachedHumidity;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        // Serial.println("SensorManager WARN: getHumidity - Mutex timeout or invalid.");
    }
    return hum;
}

float SensorManager::getSoilHumidity() const { // IMPLEMENTAÇÃO DO NOVO GETTER
    float soilHum = NAN;
     if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        soilHum = this->cachedSoilHumidity;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        Serial.println("SensorManager WARN: getSoilHumidity - Mutex timeout or invalid.");
    }
    return soilHum;
}

float SensorManager::getVpd() const { // IMPLEMENTAÇÃO DO NOVO GETTER
    float vpd = NAN;
     if (!initialized) return NAN;

    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        vpd = this->cachedVpd;
        xSemaphoreGive(sensorDataMutex.get());
    } else {
        Serial.println("SensorManager WARN: getVpd - Mutex timeout or invalid.");
    }
    return vpd;
}


// --- Leitura Interna de Sensores e Cálculos (Métodos Privados) ---

float SensorManager::_readTemperatureFromSensor() {
    if (!dhtSensor) { return NAN; }
    return dhtSensor->readTemperature();
}

float SensorManager::_readHumidityFromSensor() {
     if (!dhtSensor) { return NAN; }
     return dhtSensor->readHumidity();
}

float SensorManager::_readSoilHumidityFromSensor() const {
    const int NUM_READINGS = 5;
    const float ADC_MAX = 4095;
    const int MAX_READING_ATTEMPTS = NUM_READINGS * 2; // Limite para evitar bloqueio
    const int MIN_VALID_VALUE = 1; // Valor mínimo considerado válido

    std::vector<int> validReadings;
    validReadings.reserve(NUM_READINGS);
    int attempts = 0;

    // Ensure pin is configured (optional, good practice)
    pinMode(this->sensorConfig.soilHumiditySensorPin, INPUT);

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
             vTaskDelay(pdMS_TO_TICKS(1)); // Pequeno delay entre leituras
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

float SensorManager::_calculateVpd(float temp, float hum) { // RENOMEADO E PRIVADO (não estático)
    if (isnan(temp) || isnan(hum) || hum < 0.0f || hum > 100.0f) {
        return NAN; // Entradas inválidas
    }

    // Fórmula de Buck para pressão de vapor de saturação (SVP) em kPa
    float svp = 0.61078f * expf((17.27f * temp) / (temp + 237.3f)); // Usar expf para float

    // Pressão de vapor atual (AVP) em kPa
    float avp = svp * (hum / 100.0f);

    // Déficit de Pressão de Vapor (VPD)
    float vpd = svp - avp;

    // Retornar valor positivo ou NAN
    return (vpd > 0.0f ? vpd : NAN); // VPD não deve ser negativo
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

    JsonDocument sensorDoc; // Opcional: Alocar JSON doc uma vez

    while (true) {
        if (!initialized) {
             Serial.println("SensorTask ERROR: Manager is no longer initialized. Stopping task.");
             break; // Sai do loop while(true), tarefa será deletada
        }

        // --- 1. Ler Sensores ---
        float currentTemperature = _readTemperatureFromSensor();
        float currentAirHumidity = _readHumidityFromSensor();
        float currentSoilHumidity = _readSoilHumidityFromSensor(); // Chama o método privado

        // --- 2. Calcular VPD ---
        // Chamar _calculateVpd após ter as leituras de temp/hum do ciclo atual
        float currentVpd = _calculateVpd(currentTemperature, currentAirHumidity); // Chama o método privado

        // --- 3. Atualizar Cache (AGORA INCLUI SOLO E VPD) ---
        if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
            if (!isnan(currentTemperature)) { this->cachedTemperature = currentTemperature; }
            if (!isnan(currentAirHumidity)) { this->cachedHumidity = currentAirHumidity; }
            if (!isnan(currentSoilHumidity)) { this->cachedSoilHumidity = currentSoilHumidity; } // Atualiza cache solo
            if (!isnan(currentVpd)) { this->cachedVpd = currentVpd; }                   // Atualiza cache VPD
            xSemaphoreGive(sensorDataMutex.get());
        } else {
             Serial.println("SensorTask WARN: Failed acquire mutex to update cache.");
             // Considerar se deve continuar sem atualizar cache ou tentar novamente?
             // Por agora, continua, os valores antigos permanecem no cache.
        }


        // --- 4. Processar/Publicar ---
        bool validReadings = !isnan(currentTemperature) &&
                             !isnan(currentAirHumidity) &&
                             !isnan(currentSoilHumidity) && // Incluído na validação
                             !isnan(currentVpd);           // Incluído na validação

        if (validReadings) {
            // Publicação MQTT via MqttManager injetado
            if (this->mqttManager != nullptr ) {
                // // Publicar valores individuais (agora usando as variáveis locais do ciclo)
                // this->mqttManager->publish("sensors/temperature", currentTemperature);
                // this->mqttManager->publish("sensors/air_humidity", currentAirHumidity);
                // this->mqttManager->publish("sensors/soil_humidity", currentSoilHumidity);
                // this->mqttManager->publish("sensors/vpd", currentVpd);

                // Ou publicar um único JSON
                sensorDoc.clear();
                sensorDoc["temperature"] = round(currentTemperature * 10.0) / 10.0;
                sensorDoc["airHumidity"] = round(currentAirHumidity);
                sensorDoc["soilHumidity"] = round(currentSoilHumidity);
                sensorDoc["vpd"] = round(currentVpd * 100.0) / 100.0;
                String payload;
                serializeJson(sensorDoc, payload);
                this->mqttManager->publish("sensors", payload.c_str());
                
            }
        } else {
            Serial.println("SensorTask WARN: Invalid sensor readings obtained in this cycle.");
            // Considerar publicar um status de erro ou valores NAN aqui?
        }

        // --- 5. Atualizar Display via DisplayManager injetado ---
        if (this->displayManager != nullptr && this->displayManager->isInitialized()) {
            // Passa as leituras do ciclo atual (display trata NAN)
            // Note: showSensorData pode não mostrar VPD, ajuste se necessário.
            this->displayManager->showSensorData(currentTemperature, currentAirHumidity, currentSoilHumidity);
        }

        // --- 6. Aguardar próximo ciclo ---
        vTaskDelay(SENSOR_READ_INTERVAL_MS);
    }
}

} // namespace GrowController