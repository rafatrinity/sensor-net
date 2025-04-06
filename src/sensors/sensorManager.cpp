// src/sensors/sensorManager.cpp
#include "sensorManager.hpp"
#include "targets.hpp"
#include "config.hpp"

#include <vector>
#include <numeric>
#include <Arduino.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // <<< ADICIONADO para Mutex

// Includes para a tarefa readSensors (mantidos por enquanto)
#include <ArduinoJson.h>
#include "network/mqtt.hpp"
#include "ui/displayManager.hpp"

// Variáveis Globais Externas (Dependências da tarefa readSensors)
extern TargetValues target;
extern JsonDocument doc;
extern PubSubClient mqttClient;
extern SemaphoreHandle_t mqttMutex; // Mutex do MQTT, não o nosso novo

// --- Variáveis Estáticas do Módulo ---
static DHT* dht_sensor = nullptr;
static SensorConfig sensor_config; // Configuração dos sensores
static float cached_temperature = NAN; // <<< ADICIONADO: Cache para temperatura
static float cached_humidity = NAN;    // <<< ADICIONADO: Cache para umidade
static SemaphoreHandle_t sensorDataMutex = nullptr; // <<< ADICIONADO: Mutex para proteger o cache

// --- Funções Auxiliares Internas (Opcional, ou lógica dentro da tarefa) ---
/**
 * @brief Realiza a leitura direta do sensor de temperatura DHT.
 * @warning NÃO É THREAD-SAFE. Deve ser chamada apenas pela tarefa readSensors.
 * @return float Temperatura em Celsius ou NAN em caso de falha.
 */
static float _readTemperatureFromSensor() {
    if (dht_sensor == nullptr) return NAN;
    float t = dht_sensor->readTemperature();
    // A biblioteca DHT já retorna NAN em caso de falha na leitura
    // Não precisamos checar isnan aqui e retornar -999 como antes.
    return t;
}

/**
 * @brief Realiza a leitura direta do sensor de umidade DHT.
 * @warning NÃO É THREAD-SAFE. Deve ser chamada apenas pela tarefa readSensors.
 * @return float Umidade em % ou NAN em caso de falha.
 */
static float _readHumidityFromSensor() {
    if (dht_sensor == nullptr) return NAN;
    float h = dht_sensor->readHumidity();
    return h;
}


/**
 * @brief Inicializa os sensores e o mutex de cache.
 */
void initializeSensors(const SensorConfig& config) {
    Serial.println("Initializing Sensors and Cache...");
    sensor_config = config; // Guarda a configuração

    // Inicializa o Mutex para os dados cacheados
    if (sensorDataMutex == nullptr) { // Evita recriar se chamado múltiplas vezes
       sensorDataMutex = xSemaphoreCreateMutex();
       if (sensorDataMutex == nullptr) {
           Serial.println("SensorManager FATAL ERROR: Failed to create sensorDataMutex!");
           // Lidar com o erro - talvez parar a inicialização?
           return; // Ou abort() ?
       }
    }

    // Inicializa o sensor DHT
    if (dht_sensor != nullptr) {
        delete dht_sensor;
    }
    dht_sensor = new DHT(config.dhtPin, config.dhtType);
    if (dht_sensor != nullptr) {
        dht_sensor->begin();
        Serial.println("DHT Sensor Initialized.");
    } else {
        Serial.println("ERROR: Failed to allocate DHT sensor!");
    }
     Serial.println("Sensors Initialized.");
}

// <<< FUNÇÕES readTemperature() e readHumidity() PÚBLICAS REMOVIDAS >>>

/**
 * @brief Obtém a última leitura de temperatura válida armazenada em cache.
 */
float getCurrentTemperature() {
    float temp = NAN;
    if (sensorDataMutex != nullptr && xSemaphoreTake(sensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        temp = cached_temperature;
        xSemaphoreGive(sensorDataMutex);
    } else {
        // Log opcional se falhar em obter o mutex rapidamente
        // Serial.println("WARN: Failed to get sensorDataMutex for getCurrentTemperature");
    }
    return temp;
}

/**
 * @brief Obtém a última leitura de umidade do ar válida armazenada em cache.
 */
float getCurrentHumidity() {
    float hum = NAN;
     if (sensorDataMutex != nullptr && xSemaphoreTake(sensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        hum = cached_humidity;
        xSemaphoreGive(sensorDataMutex);
    } else {
         // Log opcional
        // Serial.println("WARN: Failed to get sensorDataMutex for getCurrentHumidity");
    }
    return hum;
}


/**
 * @brief Lê a umidade do solo do sensor analógico. (Função inalterada)
 */
float readSoilHumidity(int soilPin)
{
    const int numReadings = 10;
    const float adcMax = 4095.0;
    std::vector<int> validReadings;
    validReadings.reserve(numReadings);

    for (int i = 0; i < numReadings; i++)
    {
        int analogValue = adcMax - analogRead(soilPin);
        if (analogValue > 0)
        {
            validReadings.push_back(analogValue);
        }
    }

    if (!validReadings.empty())
    {
        double sum = std::accumulate(validReadings.begin(), validReadings.end(), 0.0);
        float average = sum / validReadings.size();
        float percentage = (average / adcMax) * 100.0;
        percentage = constrain(percentage, 0.0, 100.0);
        return percentage;
    }
    else
    {
        Serial.println(F("SensorManager: Failed to get valid readings from Soil Humidity sensor!"));
        return NAN; // Retornar NAN para consistência com outras leituras
    }
}

/**
 * @brief Calcula o Déficit de Pressão de Vapor (VPD). (Função inalterada)
 */
float calculateVpd(float tem, float hum)
{
    if (isnan(tem) || isnan(hum)) // Checa por NAN
    {
        return NAN;
    }
    hum = constrain(hum, 0.0, 100.0);
    float es = 6.112 * exp((17.67 * tem) / (tem + 243.5));
    float ea = (hum / 100.0) * es;
    float vpd_hpa = es - ea;
    float vpd_kpa = vpd_hpa / 10.0;
    return vpd_kpa;
}


/**
 * @brief Tarefa FreeRTOS para leitura periódica dos sensores e atualização do cache.
 */
void readSensors(void *parameter)
{
    const AppConfig* config = static_cast<const AppConfig*>(parameter);
    // Ajustar delay se necessário, 3s é geralmente ok para DHT22 se for a única tarefa lendo
    const TickType_t loopDelay = pdMS_TO_TICKS(3000);
    Serial.println("Sensor Reading Task started (Centralized DHT Read).");

    while (true)
    {
        // --- 1. Ler Sensores (DHT e Solo) ---
        // Somente esta tarefa lê o DHT agora
        float currentTemperature = _readTemperatureFromSensor(); // Leitura direta
        float currentAirHumidity = _readHumidityFromSensor();    // Leitura direta
        float currentSoilHumidity = readSoilHumidity(sensor_config.soilHumiditySensorPin); // Leitura do solo

        // --- 2. Atualizar Cache (Thread-Safe) ---
        if (sensorDataMutex != nullptr && xSemaphoreTake(sensorDataMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            // Atualiza o cache APENAS se a leitura do DHT foi válida (não NAN)
            if (!isnan(currentTemperature)) {
                cached_temperature = currentTemperature;
            }
            if (!isnan(currentAirHumidity)) {
                 cached_humidity = currentAirHumidity;
            }
            // Se a leitura falhou (NAN), o cache mantém o último valor válido.
            // Você pode optar por atualizar para NAN aqui se preferir que getCurrent* retorne NAN após uma falha.
            // else { cached_temperature = NAN; cached_humidity = NAN; } // Opção: invalidar cache na falha

            xSemaphoreGive(sensorDataMutex);
        } else {
             Serial.println("SensorTask WARN: Failed acquire sensorDataMutex to update cache.");
        }

        // --- 3. Calcular VPD (usando os valores lidos *nesta* iteração) ---
        float currentVpd = calculateVpd(currentTemperature, currentAirHumidity);

        // --- 4. Processar e Enviar Dados (MQTT) ---
        // Usar os valores lidos nesta iteração (current*) para consistência
        if (!isnan(currentTemperature) && !isnan(currentAirHumidity) && !isnan(currentSoilHumidity) && !isnan(currentVpd))
        {
            // A lógica de garantir conexão MQTT pode permanecer aqui por enquanto,
            // mas idealmente seria abstraída.
            ensureMQTTConnection(config->mqtt);

            doc.clear();
            doc["temperature"] = currentTemperature; // Usar valor lido nesta iteração
            doc["airHumidity"] = currentAirHumidity; // Usar valor lido nesta iteração
            doc["soilHumidity"] = currentSoilHumidity;
            doc["vpd"] = currentVpd;

            String payload;
            serializeJson(doc, payload);

            if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                String topic = String(config->mqtt.roomTopic) + "/sensors";
                if (!mqttClient.publish(topic.c_str(), payload.c_str(), true))
                {
                    Serial.println("SensorTask: Failed to publish sensor data.");
                }
                xSemaphoreGive(mqttMutex);
            }
            else
            {
                Serial.println("SensorTask: Failed to acquire MQTT mutex for publishing.");
            }
        }
        else
        {
            Serial.println("SensorTask: Invalid sensor readings obtained, skipping MQTT publish for this cycle.");
            // Logar quais leituras falharam pode ser útil
             Serial.printf(" Readings: T=%.1f, AH=%.1f, SH=%.1f, VPD=%.2f\n",
                           currentTemperature, currentAirHumidity, currentSoilHumidity, currentVpd);
        }

        // --- 5. Atualizar Display ---
        // Passar os valores lidos nesta iteração para o display
        displayManagerShowSensorData(currentTemperature, currentAirHumidity, currentSoilHumidity);

        // --- 6. Aguardar próximo ciclo ---
        vTaskDelay(loopDelay);
    }
}