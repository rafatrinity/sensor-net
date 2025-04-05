#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include <time.h>              // Para struct tm (necessário para TargetValues)
#include "sensors/targets.hpp" // Para a definição da struct TargetValues
#include "config.hpp"          // Para a definição da struct GPIOControlConfig

/**
 * @brief Inicializa os pinos GPIO definidos na configuração como OUTPUT.
 *
 * @param config Referência para a estrutura de configuração dos pinos de controle.
 */
void initializeActuators(const GPIOControlConfig& config);

/**
 * @brief Verifica a hora atual contra os horários alvo e controla o pino da luz.
 *
 * Obtém a hora atual (usando getLocalTime internamente por enquanto) e compara
 * com os horários lightOn e lightOff para determinar se a luz deve estar acesa
 * ou apagada, atualizando o estado do pino GPIO correspondente.
 *
 * @param lightOn Estrutura tm representando o horário para ligar a luz.
 * @param lightOff Estrutura tm representando o horário para desligar a luz.
 * @param lightPin O número do pino GPIO que controla a luz.
 */
void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin);

/**
 * @brief Função da tarefa FreeRTOS para controle periódico da iluminação.
 *
 * Esta tarefa chama checkAndControlLight() em intervalos regulares para garantir
 * que o estado da luz seja atualizado conforme a hora do dia e as configurações alvo.
 *
 * @param parameter Ponteiro para parâmetros (não utilizado atualmente).
 */
void lightControlTask(void *parameter);

// --- Funções futuras ---
// void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin);
// void humidityControlTask(void* parameter);
// -----------------------

#endif // ACTUATOR_MANAGER_HPP