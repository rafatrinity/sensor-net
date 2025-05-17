// src/sensors/sensorManager.cpp
#include "sensorManager.hpp"
#include "ui/displayManager.hpp"
#include "network/mqttManager.hpp"
#include "data/dataHistoryManager.hpp"
#include <Arduino.h>
#include <math.h>
#include <vector>
#include <numeric>
#include <ArduinoJson.h>
#include "config.hpp"
#include "utils/logger.hpp"
#include "utils/timeService.hpp"
#include "data/historicDataPoint.hpp"

namespace GrowController {

const TickType_t SensorManager::SENSOR_READ_INTERVAL_MS = pdMS_TO_TICKS(10000);
const TickType_t SensorManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(500);

// --- Construtor e Destrutor ---

SensorManager::SensorManager(const SensorConfig& config,
                             TimeService& timeSvc,
                             DataHistoryManager* historyMgr,
                             DisplayManager* displayMgr,
                             MqttManager* mqttMgr) :
    sensorConfig(config),
    timeServiceRef(timeSvc),                  
    dataHistoryManagerPtr(historyMgr),        
    displayManager(displayMgr),
    mqttManager(mqttMgr),
    dhtSensor(nullptr),
    cachedTemperature(NAN),
    cachedHumidity(NAN),
    cachedSoilHumidity(NAN),
    cachedVpd(NAN),
    readTaskHandle(nullptr),
    initialized(false),
    lastSaveToFlashMillis(0)
{
    // Logger::info("SensorManager: Constructor called.");
}

SensorManager::~SensorManager() {
    if (readTaskHandle != nullptr) {
        // Logger::info("SensorManager: Stopping sensor task...");
        vTaskDelete(readTaskHandle);
        readTaskHandle = nullptr;
    }
    // Logger::info("SensorManager: Destroyed.");
}

bool SensorManager::initialize() {
    if (initialized) {
       Logger::warn("SensorManager: Already initialized.");
       return true;
   }
   Logger::info("SensorManager: Initializing...");

   if (!sensorDataMutex) {
       Logger::error("SensorManager: Failed to create data mutex! Cannot initialize.");
       return false;
   }
   // Logger::debug("SensorManager: Data mutex verified.");

   for (int attempt = 1; attempt <= INIT_RETRY_COUNT; ++attempt) {
       // Logger::debug("SensorManager: Attempt %d/%d to initialize DHT sensor...", attempt, INIT_RETRY_COUNT);

       dhtSensor.reset(new (std::nothrow) DHT(sensorConfig.dhtPin, sensorConfig.dhtType));
       if (!dhtSensor) {
           Logger::error("SensorManager ERROR: Failed to allocate DHT sensor object!");
            if (attempt < INIT_RETRY_COUNT) {
                vTaskDelay(pdMS_TO_TICKS(INIT_RETRY_DELAY_MS));
            }
           continue;
       }

       dhtSensor->begin(); // begin() não retorna status

       float initialTemp = dhtSensor->readTemperature(); // Leitura de teste
       float initialHum = dhtSensor->readHumidity();   // Leitura de teste

       if (isnan(initialTemp) || isnan(initialHum)) {
           Logger::warn("SensorManager: DHT sensor did not return valid data on init attempt %d.", attempt);
           dhtSensor.reset(); // Libera o objeto falho
           if (attempt < INIT_RETRY_COUNT) {
               vTaskDelay(pdMS_TO_TICKS(INIT_RETRY_DELAY_MS));
           }
       } else {
           Logger::info("SensorManager: DHT Sensor Initialized successfully (Initial read: %.1fC, %.1f%%).", initialTemp, initialHum);
           initialized = true;
           // Logger::info("SensorManager: Initialization successful.");
           return true; // Sai do loop e da função com sucesso
       }
   }

   Logger::error("SensorManager ERROR: DHT Initialization failed after all retries.");
   dhtSensor.reset(); // Garante que o ponteiro é nulo se falhar
   initialized = false;
   return false;
}

// Modificar runSensorTask
void SensorManager::runSensorTask() {
    Logger::info("SensorManager: runSensorTask loop entered. Read interval: %lu ms, Save interval: %lu ms.",
                 (unsigned long)SENSOR_READ_INTERVAL_MS / portTICK_PERIOD_MS, SAVE_INTERVAL_MILLIS);

    // Inicializa lastSaveToFlashMillis para que o primeiro salvamento ocorra após o primeiro intervalo
    lastSaveToFlashMillis = millis();

    while (true) {
        if (!initialized) {
             Logger::error("SensorTask ERROR: Manager is no longer initialized. Exiting task.");
             break; // Sai do loop while(true), tarefa será deletada
        }

        unsigned long currentMillisCycle = millis(); // Obter millis uma vez por ciclo

        // --- 1. Ler Sensores ---
        float currentTemperature = _readTemperatureFromSensor();
        float currentAirHumidity = _readHumidityFromSensor();
        float currentSoilHumidity = _readSoilHumidityFromSensor();
        float currentVpd = _calculateVpd(currentTemperature, currentAirHumidity);

        // Logger::debug("[SensorTask] Raw - T:%.1f, AH:%.1f, SH:%.1f, VPD:%.2f",
        //             currentTemperature, currentAirHumidity, currentSoilHumidity, currentVpd);

        // --- 2. Acumular leituras para média ---
        if (!isnan(currentTemperature)) {
            currentTempSum += currentTemperature;
            validTempReadings++;
        }
        if (!isnan(currentAirHumidity)) {
            currentAirHumSum += currentAirHumidity;
            validAirHumReadings++;
        }
        if (!isnan(currentSoilHumidity)) {
            currentSoilHumSum += currentSoilHumidity;
            validSoilHumReadings++;
        }
        // VPD não é somado diretamente, será recalculado a partir das médias de T e H.

        // --- 3. Atualizar Cache com leituras instantâneas ---
        if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
            if (!isnan(currentTemperature)) { this->cachedTemperature = currentTemperature; }
            if (!isnan(currentAirHumidity)) { this->cachedHumidity = currentAirHumidity; }
            if (!isnan(currentSoilHumidity)) { this->cachedSoilHumidity = currentSoilHumidity; }
            if (!isnan(currentVpd)) { this->cachedVpd = currentVpd; }
            xSemaphoreGive(sensorDataMutex.get());
        } else {
             // Logger::warn("SensorTask WARN: Failed acquire mutex to update cache.");
        }

        // --- 4. Processar/Publicar leituras instantâneas (MQTT, Display) ---
        bool validInstantReadings = !isnan(currentTemperature) && !isnan(currentAirHumidity);
                                  // SoilHumidity e VPD podem ser NAN e ainda assim T/AH serem válidos.

        if (this->mqttManager != nullptr ) { // Publicar mesmo se alguns forem NAN, o broker/cliente trata
            if(!isnan(currentTemperature)) this->mqttManager->publish("sensors/temperature", currentTemperature);
            if(!isnan(currentAirHumidity)) this->mqttManager->publish("sensors/air_humidity", currentAirHumidity);
            if(!isnan(currentSoilHumidity)) this->mqttManager->publish("sensors/soil_humidity", currentSoilHumidity);
            if(!isnan(currentVpd)) this->mqttManager->publish("sensors/vpd", currentVpd);
        }

        if (this->displayManager != nullptr && this->displayManager->isInitialized()) {
            this->displayManager->showSensorData(currentTemperature, currentAirHumidity, currentSoilHumidity);
        }

        // --- 5. Verificar se é hora de salvar a média na Flash ---
        if (currentMillisCycle - lastSaveToFlashMillis >= SAVE_INTERVAL_MILLIS) {
            Logger::info("SensorTask: Save interval reached. Calculating and saving averages.");
            HistoricDataPoint dp;
            struct tm timeinfo; // struct tm para mktime

            // Obter timestamp
            if (timeServiceRef.getCurrentTime(timeinfo)) {
                dp.timestamp = mktime(&timeinfo); // mktime converte tm local para Unix timestamp UTC
                // Logger::debug("SensorTask: Timestamp for save: %lu", dp.timestamp);
            } else {
                dp.timestamp = 0; // Indica timestamp inválido/não disponível
                Logger::warn("SensorTask: Failed to get current time for historic data point. Timestamp set to 0.");
            }

            // Calcular médias
            dp.avgTemperature = (validTempReadings > 0) ? (currentTempSum / validTempReadings) : NAN;
            dp.avgAirHumidity = (validAirHumReadings > 0) ? (currentAirHumSum / validAirHumReadings) : NAN;
            dp.avgSoilHumidity = (validSoilHumReadings > 0) ? (currentSoilHumSum / validSoilHumReadings) : NAN;
            
            // Calcular VPD a partir das médias de Temperatura e Umidade do Ar
            if (validTempReadings > 0 && validAirHumReadings > 0 && !isnan(dp.avgTemperature) && !isnan(dp.avgAirHumidity)) {
                 dp.avgVpd = _calculateVpd(dp.avgTemperature, dp.avgAirHumidity);
            } else {
                 dp.avgVpd = NAN;
            }

            Logger::info("SensorTask: Averages to save - T:%.1f, AH:%.1f, SH:%.1f, VPD:%.2f (TS: %lu)",
                         dp.avgTemperature, dp.avgAirHumidity, dp.avgSoilHumidity, dp.avgVpd, dp.timestamp);

            // Salvar ponto de dado histórico
            if (dataHistoryManagerPtr) {
                if (dataHistoryManagerPtr->addDataPoint(dp)) {
                    Logger::info("SensorTask: Historic data point saved successfully.");
                } else {
                    Logger::error("SensorTask: Failed to save historic data point.");
                }
            } else {
                Logger::warn("SensorTask: DataHistoryManager is null. Cannot save historic data.");
            }

            // Resetar somas e contagens para o próximo intervalo
            currentTempSum = 0.0f;
            validTempReadings = 0;
            currentAirHumSum = 0.0f;
            validAirHumReadings = 0;
            currentSoilHumSum = 0.0f;
            validSoilHumReadings = 0;
            // VPD não precisa resetar soma, pois é recalculado

            lastSaveToFlashMillis = currentMillisCycle; // Atualiza o tempo do último salvamento
        }

        // --- 6. Aguardar próximo ciclo de leitura ---
        vTaskDelay(SENSOR_READ_INTERVAL_MS);
    }
}

// _readTemperatureFromSensor, _readHumidityFromSensor, _readSoilHumidityFromSensor, _calculateVpd
// startSensorTask, readSensorsTaskWrapper, getters (getTemperature etc.)
// permanecem como no seu código original ou com pequenas adaptações já feitas.

float SensorManager::_readTemperatureFromSensor() {
    if (!dhtSensor) { return NAN; }
    return dhtSensor->readTemperature(); // Adicionar false para não ler umidade se for DHT11/21/22
}

float SensorManager::_readHumidityFromSensor() {
     if (!dhtSensor) { return NAN; }
     return dhtSensor->readHumidity();
}

float SensorManager::_readSoilHumidityFromSensor() const {
    const int NUM_READINGS = 5;
    const float ADC_MAX = 4095.0f;
    // Valores de calibração (EXEMPLOS! DEVEM SER AJUSTADOS PARA SEU SENSOR)
    // ADC mais alto = mais seco, ADC mais baixo = mais molhado (para sensores resistivos comuns)
    const float SOIL_ADC_AIR_VALUE = 3200.0f;  // Valor ADC do sensor no ar (0% umidade)
    const float SOIL_ADC_WATER_VALUE = 1200.0f; // Valor ADC do sensor na água (100% umidade)

    std::vector<int> readings;
    readings.reserve(NUM_READINGS);

    for (int i = 0; i < NUM_READINGS; ++i) {
        readings.push_back(analogRead(this->sensorConfig.soilHumiditySensorPin));
        if (i < NUM_READINGS - 1) vTaskDelay(pdMS_TO_TICKS(20)); // Pequeno delay
    }

    // Remover outliers simples (opcional, mas pode ajudar)
    // std::sort(readings.begin(), readings.end());
    // if (NUM_READINGS >= 5) { // Pega os 3 do meio de 5
    //     sum = readings[1] + readings[2] + readings[3];
    //     average = sum / 3.0;
    // } else { ... }

    double sum = std::accumulate(readings.begin(), readings.end(), 0.0);
    if (readings.empty()) return NAN;
    
    float averageAdc = static_cast<float>(sum) / readings.size();

    // Mapear o valor ADC para porcentagem
    float percentage = 100.0f * (SOIL_ADC_AIR_VALUE - averageAdc) / (SOIL_ADC_AIR_VALUE - SOIL_ADC_WATER_VALUE);
    
    // Limitar entre 0% e 100%
    percentage = constrain(percentage, 0.0f, 100.0f);
    return percentage;
}

float SensorManager::_calculateVpd(float temp, float hum) {
    if (isnan(temp) || isnan(hum) || hum < 0.0f || hum > 100.0f || temp < -20.0f || temp > 70.0f ) {
        return NAN;
    }
    // Pressão de vapor de saturação (SVP) em kPa usando a fórmula de Tetens ou similar.
    // SVP(Pa) = 610.78 * exp((17.27 * T_celsius) / (T_celsius + 237.3))
    // Para kPa, dividir por 1000
    float svp_kpa = 0.61078f * expf((17.27f * temp) / (temp + 237.3f));

    // Pressão de vapor atual (AVP) em kPa
    float avp_kpa = svp_kpa * (hum / 100.0f);

    // Déficit de Pressão de Vapor (VPD)
    float vpd = svp_kpa - avp_kpa;
    return (vpd >= 0.0f ? vpd : 0.0f); // VPD não deve ser negativo
}

float SensorManager::getTemperature() const {
    float temp = NAN;
    if (!initialized) return NAN;
    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        temp = this->cachedTemperature;
        xSemaphoreGive(sensorDataMutex.get());
    }
    return temp;
}

float SensorManager::getHumidity() const {
    float hum = NAN;
     if (!initialized) return NAN;
    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        hum = this->cachedHumidity;
        xSemaphoreGive(sensorDataMutex.get());
    }
    return hum;
}

float SensorManager::getSoilHumidity() const {
    float soilHum = NAN;
     if (!initialized) return NAN;
    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        soilHum = this->cachedSoilHumidity;
        xSemaphoreGive(sensorDataMutex.get());
    }
    return soilHum;
}

float SensorManager::getVpd() const {
    float vpd_val = NAN;
     if (!initialized) return NAN;
    if (sensorDataMutex && xSemaphoreTake(sensorDataMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        vpd_val = this->cachedVpd;
        xSemaphoreGive(sensorDataMutex.get());
    }
    return vpd_val;
}

bool SensorManager::isInitialized() const {
    return initialized;
}

bool SensorManager::startSensorTask(UBaseType_t priority, uint32_t stackSize) {
    if (!initialized) {
         Logger::error("SensorManager: Cannot start task, manager not initialized.");
         return false;
    }
    if (readTaskHandle != nullptr) {
        Logger::warn("SensorManager: Sensor task already started.");
        return true;
    }

    // Logger::info("SensorManager: Starting sensor reading task...");
    BaseType_t result = xTaskCreate(
        readSensorsTaskWrapper,
        "SensorTask",
        stackSize,
        this,
        priority,
        &this->readTaskHandle
    );

    if (result != pdPASS) {
        readTaskHandle = nullptr;
        Logger::error("SensorManager: Failed to create sensor task! Error code: %d", result);
        return false;
    }

    Logger::info("SensorManager: Sensor task started successfully.");
    return true;
}

void SensorManager::readSensorsTaskWrapper(void *pvParameters) {
    SensorManager* instance = static_cast<SensorManager*>(pvParameters);
    if (instance != nullptr) {
        instance->runSensorTask();
    } else {
        // Logger::error("SensorTaskWrapper ERROR: Received null instance pointer! Task cannot run.");
    }
    // Logger::info("SensorTaskWrapper: Task loop exited. Deleting task.");
    vTaskDelete(NULL); // Auto-deleta a tarefa ao sair do loop
}

} // namespace GrowController