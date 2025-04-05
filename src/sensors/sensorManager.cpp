#include "sensorManager.hpp"
#include "targets.hpp" // Usado em readHumidity (temporariamente) e na tarefa readSensors (global)
#include "config.hpp"  // Usado para obter pinos e configurações (global)

#include <vector>
#include <numeric>
#include <Arduino.h>
#include <math.h>              // Para exp() em calculateVpd
#include "freertos/FreeRTOS.h" // Para vTaskDelay na tarefa
#include "freertos/task.h"

// --- Includes que AINDA são necessários pela tarefa readSensors ---
// Estes deveriam ser movidos quando a tarefa for refatorada
#include <ArduinoJson.h>       // Para construir o payload na tarefa readSensors
#include "network/mqtt.hpp"    // Para ensureMQTTConnection e mqttClient na tarefa readSensors
#include "ui/displayManager.hpp"

// --- Variáveis Globais Externas (Dependências) ---
// Idealmente, estas seriam injetadas ou acessadas via interfaces/filas
extern TargetValues target;
extern JsonDocument doc;
extern PubSubClient mqttClient;
extern SemaphoreHandle_t mqttMutex;

// --- Variáveis Estáticas do Módulo --- 
static DHT* dht_sensor = nullptr;
static SensorConfig sensor_config;
// ---------------------------

/**
 * @brief Inicializa os sensores gerenciados por este módulo.
 * Atualmente, inicializa apenas o sensor DHT.
 */
void initializeSensors(const SensorConfig& config) {
    Serial.println("Initializing DHT Sensor...");
    sensor_config = config;
    if (dht_sensor != nullptr) {
        delete dht_sensor;
        dht_sensor = nullptr;
    }
    dht_sensor = new DHT(config.dhtPin, config.dhtType);
    if (dht_sensor != nullptr) {
        dht_sensor->begin(); // Chama begin na nova instância
        Serial.println("DHT Sensor Initialized.");
    } else {
        Serial.println("ERROR: Failed to allocate DHT sensor!");
    }
}

/**
 * @brief Lê a temperatura do sensor DHT.
 *
 * @return float A temperatura lida em graus Celsius, ou -999.0 em caso de falha na leitura.
 */
float readTemperature()
{
    if (dht_sensor == nullptr) {
        Serial.println(F("SensorManager ERROR: DHT sensor not initialized!"));
        return -999.0;
    }

    float temperature = dht_sensor->readTemperature();
    
    if (isnan(temperature))
    {
        Serial.println(F("SensorManager: Failed to read temperature from DHT sensor!"));
        return -999.0;
    }
    return temperature;
}

/**
 * @brief Lê a umidade relativa do ar do sensor DHT.
 *
 * @return float A umidade relativa lida em porcentagem (%), ou -999.0 em caso de falha na leitura.
 */
float readHumidity()
{
    if (dht_sensor == nullptr) {
        Serial.println(F("SensorManager ERROR: DHT sensor not initialized!"));
        return -999.0;
    }

    float humidity = dht_sensor->readHumidity();
    if (isnan(humidity))
    {
        Serial.println(F("SensorManager: Failed to read humidity from DHT sensor!"));
        return -999.0;
    }
    return humidity;
}

/**
 * @brief Lê a umidade do solo do sensor analógico.
 * Realiza múltiplas leituras, calcula a média e converte para uma escala percentual (0-100).
 *
 * @return float A umidade do solo calculada em porcentagem (%), ou 0.0 se nenhuma leitura válida for obtida.
 */
float readSoilHumidity(int soilPin)
{
    const int numReadings = 10; // Número de amostras para a média
    const float adcMax = 4095.0; // Valor máximo do ADC (12 bits no ESP32)
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
        return 0.0;
    }
}

/**
 * @brief Calcula o Déficit de Pressão de Vapor (VPD) com base na temperatura e umidade.
 *
 * @param tem A temperatura em graus Celsius.
 * @param hum A umidade relativa em porcentagem (%).
 * @return float O valor do VPD calculado em kiloPascals (kPa), ou NAN se as entradas forem inválidas.
 */
float calculateVpd(float tem, float hum)
{
    if (tem <= -999.0 || hum <= -999.0 || isnan(tem) || isnan(hum))
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
 * @brief Tarefa FreeRTOS para leitura periódica dos sensores e ações relacionadas.
 *
 * @param parameter Ponteiro genérico para parâmetros da tarefa (não utilizado aqui).
 */
void readSensors(void *parameter)
{
    const AppConfig* config = static_cast<const AppConfig*>(parameter);
    const TickType_t loopDelay = pdMS_TO_TICKS(3000);
    Serial.println("Sensor Reading Task started.");

    while (true)
    {
        float temperature = readTemperature();
        float airHumidity = readHumidity();
        float soilHumidity = readSoilHumidity(sensor_config.soilHumiditySensorPin);
        float vpd = calculateVpd(temperature, airHumidity);

        if (!isnan(temperature) && !isnan(airHumidity) && !isnan(soilHumidity) && !isnan(vpd))
        {
            ensureMQTTConnection(config->mqtt);

            doc.clear();
            doc["temperature"] = temperature;
            doc["airHumidity"] = airHumidity;
            doc["soilHumidity"] = soilHumidity;
            doc["vpd"] = vpd;

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
            Serial.println("SensorTask: Invalid sensor readings, skipping MQTT publish.");
        }

        displayManagerShowSensorData(temperature, airHumidity, soilHumidity);
        vTaskDelay(loopDelay);
    }
}
