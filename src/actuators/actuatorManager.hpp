// src/actuators/actuatorManager.hpp (Corrigido)
#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include <time.h>     // Para struct tm (usado em checkAndControlLight)
#include "config.hpp" // Para a definição da struct GPIOControlConfig

// Estrutura definida no .cpp, não precisa ser exposta aqui
// struct ActuatorTaskParams;

/**
 * @brief Inicializa os pinos GPIO definidos na configuração como OUTPUT.
 * Configura os pinos de controle de luz e umidade.
 * @param config Referência para a estrutura de configuração dos pinos de controle.
 */
void initializeActuators(const GPIOControlConfig& config);

/**
 * @brief Verifica a hora atual contra os horários alvo e controla o pino da luz.
 * Calcula tempo em minutos desde a meia-noite para comparação, lidando com horários noturnos.
 * @param lightOn Referência constante à struct tm representando o horário de LIGAR (HH:MM).
 * @param lightOff Referência constante à struct tm representando o horário de DESLIGAR (HH:MM).
 * @param lightPin O pino GPIO que controla a luz.
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
 * Recupera o horário agendado do TargetDataManager e chama checkAndControlLight.
 * @param parameter Ponteiro para a estrutura ActuatorTaskParams contendo
 *                  GPIOControlConfig e um ponteiro para TargetDataManager. // <<< CORRIGIDO
 */
void lightControlTask(void *parameter);

/**
 * @brief Função da tarefa FreeRTOS para controle periódico da umidade.
 * Recupera a umidade atual (via sensorManager) e o alvo (via TargetDataManager),
 * então chama checkAndControlHumidity().
 * @param parameter Ponteiro para a estrutura ActuatorTaskParams contendo
 *                  GPIOControlConfig e um ponteiro para TargetDataManager. // <<< CORRIGIDO
 */
void humidityControlTask(void *parameter);

#endif // ACTUATOR_MANAGER_HPP