#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include <time.h>              // Para struct tm (necessário para TargetValues)
#include "sensors/targets.hpp" // Para a definição da struct TargetValues
#include "config.hpp"          // Para a definição da struct GPIOControlConfig

/**
 * @brief Inicializa os pinos GPIO definidos na configuração como OUTPUT.
 * Configura os pinos de controle de luz e umidade.
 * @param config Referência para a estrutura de configuração dos pinos de controle.
 */
void initializeActuators(const GPIOControlConfig& config);

/**
 * @brief Verifica a hora atual contra os horários alvo e controla o pino da luz.
 * // ... (comentário existente) ...
 */
void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin);


/**
 * @brief Verifica a umidade atual contra o alvo e controla o pino do umidificador.
 * Liga o pino se a umidade atual for menor que o alvo, desliga caso contrário.
 * Assume que o pino controla um UMIDIFICADOR.
 *
 * @param currentHumidity A umidade relativa do ar atual (em %).
 * @param targetHumidity A umidade relativa do ar desejada (em %).
 * @param humidityPin O número do pino GPIO que controla o dispositivo de umidade.
 */
void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin);

/**
 * @brief Função da tarefa FreeRTOS para controle periódico da iluminação.
 * // ... (comentário existente) ...
 */
void lightControlTask(void *parameter);

/**
 * @brief Função da tarefa FreeRTOS para controle periódico da umidade.
 * Esta tarefa lê periodicamente a umidade do ar (através do sensorManager)
 * e chama checkAndControlHumidity() para atualizar o estado do atuador.
 *
 * @param parameter Ponteiro para a estrutura GPIOControlConfig.
 */
void humidityControlTask(void *parameter); // <<< ADICIONADO

#endif // ACTUATOR_MANAGER_HPP