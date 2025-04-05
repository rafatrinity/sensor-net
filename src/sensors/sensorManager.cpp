// sensorManager.cpp
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
extern AppConfig appConfig;
extern TargetValues target;
extern JsonDocument doc;
extern PubSubClient mqttClient;
extern SemaphoreHandle_t mqttMutex;

// --- Instância do Sensor ---
// Ainda global, dependendo de appConfig global. Idealmente, injetado.
DHT dht(appConfig.sensor.dhtPin, appConfig.sensor.dhtType);
// ---------------------------

/**
 * @brief Inicializa os sensores gerenciados por este módulo.
 * Atualmente, inicializa apenas o sensor DHT.
 */
void initializeSensors()
{
    Serial.println("Initializing DHT Sensor...");
    dht.begin();
    // Se houver outros sensores a inicializar (ex: ADC para umidade do solo), adicione aqui.
    Serial.println("DHT Sensor Initialized.");
}

/**
 * @brief Lê a temperatura do sensor DHT.
 *
 * @return float A temperatura lida em graus Celsius, ou -999.0 em caso de falha na leitura.
 */
float readTemperature()
{
    // Leitura pode levar até 250ms. Considerar impacto no tempo de loop.
    float temperature = dht.readTemperature(); // Tenta ler a temperatura
    if (isnan(temperature))                    // Verifica se a leitura falhou (retorna Not-a-Number)
    {
        Serial.println(F("SensorManager: Failed to read temperature from DHT sensor!"));
        return -999.0; // Retorna valor de erro
    }
    // Serial.print(F("SensorManager: Temperature Read: ")); Serial.println(temperature); // Log opcional
    return temperature; // Retorna o valor lido
}

/**
 * @brief Lê a umidade relativa do ar do sensor DHT.
 * @note A lógica de controle do umidificador foi removida desta função.
 *
 * @return float A umidade relativa lida em porcentagem (%), ou -999.0 em caso de falha na leitura.
 */
float readHumidity()
{
    // Leitura pode levar até 250ms.
    float humidity = dht.readHumidity(); // Tenta ler a umidade
    if (isnan(humidity))                 // Verifica se a leitura falhou
    {
        Serial.println(F("SensorManager: Failed to read humidity from DHT sensor!"));
        return -999.0; // Retorna valor de erro
    }

    // Serial.print(F("SensorManager: Humidity Read: ")); Serial.println(humidity); // Log opcional
    return humidity; // Retorna o valor lido
}

/**
 * @brief Lê a umidade do solo do sensor analógico.
 * Realiza múltiplas leituras, calcula a média e converte para uma escala percentual (0-100%).
 * Assume que o valor analógico diminui com o aumento da umidade (requer inversão 4095 - leitura).
 *
 * @return float A umidade do solo calculada em porcentagem (%), ou 0.0 se nenhuma leitura válida for obtida.
 */
float readSoilHumidity()
{
    const int numReadings = 10; // Número de amostras para a média
    const float adcMax = 4095.0; // Valor máximo do ADC (12 bits no ESP32)
    std::vector<int> validReadings;
    validReadings.reserve(numReadings); // Pre-aloca espaço para eficiência

    // Realiza múltiplas leituras
    for (int i = 0; i < numReadings; i++)
    {
        // Lê o valor analógico. A inversão (adcMax - leitura) depende do sensor:
        // - Sensores Capacitivos: Geralmente a tensão/leitura DIMINUI com a umidade -> Usar inversão.
        // - Sensores Resistivos: Geralmente a tensão/leitura AUMENTA com a umidade -> NÃO usar inversão.
        // Verifique o comportamento do seu sensor específico.
        int analogValue = adcMax - analogRead(appConfig.sensor.soilHumiditySensorPin);

        // Considera apenas leituras válidas (ajuste a condição se necessário)
        if (analogValue > 0) // Ignora leituras que resultaram em 0 ou menos após inversão
        {
            validReadings.push_back(analogValue);
        }
        // Pequeno delay pode ajudar a estabilizar leituras ADC, mas aumenta o tempo total.
        // delayMicroseconds(10);
    }

    // Calcula a média se houver leituras válidas
    if (!validReadings.empty())
    {
        // Usa double para a soma para evitar overflow se numReadings for grande
        double sum = std::accumulate(validReadings.begin(), validReadings.end(), 0.0);
        float average = sum / validReadings.size();

        // Converte a média (0-4095) para porcentagem (0-100)
        // Nota: Esta é uma conversão linear simples. Sensores reais podem exigir calibração
        //       com pontos de "totalmente seco" e "totalmente molhado" para maior precisão.
        //       Ex: percent = map(average, dryValue, wetValue, 0, 100);
        float percentage = (average / adcMax) * 100.0;

        // Garante que o valor esteja entre 0 e 100
        percentage = constrain(percentage, 0.0, 100.0);

        // Serial.print(F("SensorManager: Soil Humidity Read (Avg ADC): ")); Serial.println(average); // Log opcional
        // Serial.print(F("SensorManager: Soil Humidity Read (%): ")); Serial.println(percentage); // Log opcional
        return percentage;
    }
    else
    {
        Serial.println(F("SensorManager: Failed to get valid readings from Soil Humidity sensor!"));
        return 0.0; // Retorna 0 se não houver leituras válidas
    }
}

/**
 * @brief Calcula o Déficit de Pressão de Vapor (VPD) com base na temperatura e umidade.
 * A fórmula usa a equação de Tetens para a pressão de vapor de saturação (es).
 * VPD = es - ea, onde ea (pressão de vapor atual) = (humidade / 100) * es.
 *
 * @param tem A temperatura em graus Celsius.
 * @param hum A umidade relativa em porcentagem (%).
 * @return float O valor do VPD calculado em kiloPascals (kPa), ou NAN se as entradas forem inválidas.
 */
float calculateVpd(float tem, float hum)
{
    // Verifica se as entradas são válidas (não são os valores de erro)
    if (tem <= -999.0 || hum <= -999.0 || isnan(tem) || isnan(hum))
    {
        return NAN; // Retorna Not-a-Number se as entradas forem inválidas
    }

    // Garante que a umidade esteja na faixa de 0 a 100 para o cálculo
    hum = constrain(hum, 0.0, 100.0);

    // Calcula a pressão de vapor de saturação (es) em HectoPascals (hPa) / milibares (mb)
    // Fórmula de Tetens (aproximação comum)
    float es = 6.112 * exp((17.67 * tem) / (tem + 243.5));

    // Calcula a pressão de vapor atual (ea) em HectoPascals (hPa) / milibares (mb)
    float ea = (hum / 100.0) * es;

    // Calcula o VPD (es - ea)
    float vpd_hpa = es - ea;

    // Converte para kiloPascals (kPa) dividindo por 10 (1 kPa = 10 hPa/mb)
    float vpd_kpa = vpd_hpa / 10.0;

    // Serial.print(F("SensorManager: VPD Calculated (kPa): ")); Serial.println(vpd_kpa); // Log opcional
    return vpd_kpa; // Retorna VPD em kPa
}

/**
 * @brief Função da tarefa FreeRTOS para leitura periódica dos sensores e ações relacionadas.
 *
 * @warning Esta tarefa atualmente acumula múltiplas responsabilidades: Leitura, Publicação MQTT e (anteriormente) Atualização de Display.
 *          Considere refatorar usando filas para comunicação inter-tarefas e separando responsabilidades.
 *
 * @param parameter Ponteiro genérico para parâmetros da tarefa (não utilizado aqui).
 */
void readSensors(void *parameter)
{
    // Intervalo entre leituras e publicações
    const TickType_t loopDelay = pdMS_TO_TICKS(3000);


    Serial.println("Sensor Reading Task started.");

    while (true)
    {
        // --- 1. Leitura dos Sensores ---
        float temperature = readTemperature();
        float airHumidity = readHumidity();
        float soilHumidity = readSoilHumidity();
        float vpd = calculateVpd(temperature, airHumidity);

        // --- 2. Publicação MQTT (Responsabilidade Mal Localizada) ---
        if (!isnan(temperature) && !isnan(airHumidity) && !isnan(soilHumidity) && !isnan(vpd))
        { // Apenas publica se tudo for válido

            // Garante conexão MQTT (Chamada Bloqueante se desconectado!)
            ensureMQTTConnection(); // Esta função pertence ao módulo MQTT

            // Prepara o payload JSON
            // Idealmente, a tarefa receberia os dados via fila e aqui apenas formataria/publicaria
            doc.clear();
            doc["temperature"] = temperature;
            doc["airHumidity"] = airHumidity;
            doc["soilHumidity"] = soilHumidity;
            doc["vpd"] = vpd;

            String payload;
            serializeJson(doc, payload); // Cria a string JSON

            // Tenta publicar os dados, protegendo o cliente MQTT com mutex
            if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            { // Espera até 100ms pelo mutex
                String topic = String(appConfig.mqtt.roomTopic) + "/sensors";
                // Publica com retenção (true) para que novos clientes recebam o último estado
                if (!mqttClient.publish(topic.c_str(), payload.c_str(), true))
                {
                    Serial.println("SensorTask: Failed to publish sensor data.");
                    // Considerar lógica de erro aqui (ex: tentar reconectar, logar estado)
                    // Serial.print("MQTT Client State: "); Serial.println(mqttClient.state());
                }
                else
                {
                    // Serial.print("SensorTask: Published to "); Serial.print(topic); Serial.print(" : "); Serial.println(payload); // Log detalhado opcional
                }
                xSemaphoreGive(mqttMutex); // Libera o mutex
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

        // --- 3. Atualização do Display via DisplayManager --- <<< MODIFICADO
        // Chama a função do DisplayManager para mostrar os dados dos sensores.
        // A lógica de formatação e otimização (se necessária) fica dentro do displayManager.
        displayManagerShowSensorData(temperature, airHumidity, soilHumidity);

        vTaskDelay(loopDelay); // Pausa a tarefa pelo tempo definido
    }
}
