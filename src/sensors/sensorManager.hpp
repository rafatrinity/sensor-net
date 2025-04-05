#ifndef SENSOR_MANAGER_HPP
#define SENSOR_MANAGER_HPP

#include <DHT.h> // Necessário para o tipo DHT

// --- Dependência Global (Idealmente injetada no futuro) ---
/**
 * @brief Instância global externa do objeto sensor DHT.
 * @warning Esta é uma dependência global, considere usar injeção de dependência.
 */
extern DHT dht;
// ---------------------------------------------------------

// --- Funções do Módulo SensorManager ---

/**
 * @brief Inicializa os sensores gerenciados por este módulo.
 * Atualmente, inicializa apenas o sensor DHT.
 */
void initializeSensors();

/**
 * @brief Lê a temperatura do sensor DHT.
 *
 * @return float A temperatura lida em graus Celsius, ou -999.0 em caso de falha na leitura.
 */
float readTemperature();

/**
 * @brief Lê a umidade relativa do ar do sensor DHT.
 * @note A lógica de controle do umidificador foi removida desta função.
 *
 * @return float A umidade relativa lida em porcentagem (%), ou -999.0 em caso de falha na leitura.
 */
float readHumidity();

/**
 * @brief Lê a umidade do solo do sensor analógico.
 * Realiza múltiplas leituras, calcula a média e converte para uma escala percentual (0-100%).
 * Assume que o valor analógico diminui com o aumento da umidade (requer inversão 4095 - leitura).
 *
 * @return float A umidade do solo calculada em porcentagem (%), ou 0.0 se nenhuma leitura válida for obtida.
 */
float readSoilHumidity();

/**
 * @brief Calcula o Déficit de Pressão de Vapor (VPD) com base na temperatura e umidade.
 * A fórmula usa a equação de Tetens para a pressão de vapor de saturação.
 *
 * @param tem A temperatura em graus Celsius.
 * @param hum A umidade relativa em porcentagem (%).
 * @return float O valor do VPD calculado em HectoPascals (hPa) ou kilopascals (kPa) dependendo da preferência (aqui está em hPa / 10?), ou NAN se as entradas forem inválidas.
 * @note A unidade do resultado (kPa vs hPa) deve ser confirmada. Dividir por 10 sugere kPa.
 */
float calculateVpd(float tem, float hum);

/**
 * @brief Função da tarefa FreeRTOS para leitura periódica dos sensores e ações relacionadas.
 *
 * @warning Esta tarefa atualmente acumula múltiplas responsabilidades:
 *          1. Leitura dos sensores (OK).
 *          2. Garantir conexão MQTT (Deveria ser gerenciado pelo módulo MQTT).
 *          3. Construir payload JSON (Poderia ser uma função utilitária).
 *          4. Publicar dados via MQTT (Deveria ser responsabilidade do módulo MQTT, talvez recebendo dados via fila).
 *          5. Atualizar o display LCD (Deveria ser responsabilidade de um módulo de UI/Display, talvez recebendo dados via fila).
 *          Considere refatorar usando filas para comunicação inter-tarefas e separando responsabilidades.
 *
 * @param parameter Ponteiro genérico para parâmetros da tarefa (não utilizado aqui).
 */
void readSensors(void *parameter);

#endif // SENSOR_MANAGER_HPP