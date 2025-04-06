// src/sensors/sensorManager.hpp
#ifndef SENSOR_MANAGER_HPP
#define SENSOR_MANAGER_HPP

#include "config.hpp"
#include <DHT.h>
#include "freertos/FreeRTOS.h" // Para SemaphoreHandle_t
#include "freertos/semphr.h"

// --- Funções do Módulo SensorManager ---

/**
 * @brief Inicializa os sensores gerenciados por este módulo e o mecanismo de cache.
 * @param config Referência constante à configuração dos sensores.
 */
void initializeSensors(const SensorConfig& config);

/**
 * @brief Obtém a última leitura de temperatura válida armazenada em cache.
 * Esta função é segura para ser chamada de múltiplas tarefas (thread-safe).
 *
 * @return float A última temperatura lida em graus Celsius, ou NAN se nenhuma leitura válida foi feita ainda.
 */
float getCurrentTemperature(); // <<< NOVA FUNÇÃO GETTER PÚBLICA >>>

/**
 * @brief Obtém a última leitura de umidade do ar válida armazenada em cache.
 * Esta função é segura para ser chamada de múltiplas tarefas (thread-safe).
 *
 * @return float A última umidade relativa lida em porcentagem (%), ou NAN se nenhuma leitura válida foi feita ainda.
 */
float getCurrentHumidity(); // <<< NOVA FUNÇÃO GETTER PÚBLICA >>>


/**
 * @brief Lê a umidade do solo do sensor analógico.
 * (Esta função permanece inalterada, pois não envolve o DHT)
 * @param soilPin O pino ADC conectado ao sensor de umidade do solo.
 * @return float A umidade do solo calculada em porcentagem (%), ou 0.0 se nenhuma leitura válida for obtida.
 */
float readSoilHumidity(int soilPin);

/**
 * @brief Calcula o Déficit de Pressão de Vapor (VPD) com base na temperatura e umidade.
 * (Esta função permanece inalterada)
 * @param tem A temperatura em graus Celsius.
 * @param hum A umidade relativa em porcentagem (%).
 * @return float O valor do VPD calculado em kiloPascals (kPa), ou NAN se as entradas forem inválidas.
 */
float calculateVpd(float tem, float hum);

/**
 * @brief Função da tarefa FreeRTOS para leitura periódica dos sensores.
 * AGORA É A ÚNICA RESPONSÁVEL por ler o sensor DHT e atualizar o cache.
 * Continua responsável por ler o sensor de solo, calcular VPD, publicar MQTT e atualizar display.
 *
 * @param parameter Ponteiro para a estrutura AppConfig completa.
 */
void readSensors(void *parameter);

#endif // SENSOR_MANAGER_HPP