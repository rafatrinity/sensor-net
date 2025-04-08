// src/actuators/actuatorManager.hpp
#ifndef ACTUATOR_MANAGER_HPP
#define ACTUATOR_MANAGER_HPP

#include "config.hpp"               // Para GPIOControlConfig
#include "data/targetDataManager.hpp" // Para TargetDataManager
#include "sensors/sensorManager.hpp"  // Para SensorManager (Dependência!)
#include "utils/freeRTOSMutex.hpp"  // Se precisar de mutex interno (talvez não aqui)
#include <time.h>                   // Para struct tm
#include "freertos/FreeRTOS.h"      // Para FreeRTOS types
#include "freertos/task.h"          // Para TaskHandle_t

namespace GrowController {

/**
 * @brief Gerencia os atuadores (luz, umidificador) com base em horários e leituras de sensores.
 * Executa tarefas dedicadas para controle periódico.
 */
class ActuatorManager {
public:
    /**
     * @brief Construtor. Recebe configuração e dependências.
     * @param config Configuração dos pinos GPIO dos atuadores.
     * @param targetMgr Referência ao TargetDataManager para obter os valores alvo.
     * @param sensorMgr Referência ao SensorManager para obter leituras atuais (umidade).
     */
    ActuatorManager(const GPIOControlConfig& config,
                    TargetDataManager& targetMgr,
                    SensorManager& sensorMgr); // Injeta dependências

    /**
     * @brief Destrutor. Para as tarefas de controle.
     */
    ~ActuatorManager();

    // Desabilitar cópia e atribuição
    ActuatorManager(const ActuatorManager&) = delete;
    ActuatorManager& operator=(const ActuatorManager&) = delete;

    /**
     * @brief Inicializa os pinos GPIO dos atuadores.
     * @return true sempre (pode ser melhorado para verificar pinMode).
     */
    bool initialize();

    /**
     * @brief Inicia as tarefas FreeRTOS para controle de luz e umidade.
     * @param lightTaskPriority Prioridade da tarefa de luz.
     * @param humidityTaskPriority Prioridade da tarefa de umidade.
     * @param stackSize Tamanho da pilha para as tarefas.
     * @return true Se ambas as tarefas foram criadas com sucesso.
     * @return false Se a criação de alguma tarefa falhou.
     */
    bool startControlTasks(UBaseType_t lightTaskPriority = 1,
                           UBaseType_t humidityTaskPriority = 1,
                           uint32_t stackSize = 2560); // Stack maior para umidade?

    /**
      * @brief Verifica se o manager foi inicializado.
      * @return true se inicializado, false caso contrário.
      */
    bool isInitialized() const;


private:
    /**
     * @brief Verifica a hora atual contra o alvo e controla a luz.
     * @param lightOn Horário de ligar.
     * @param lightOff Horário de desligar.
     * @param lightPin Pino da luz.
     */
    void checkAndControlLight(const struct tm& lightOn, const struct tm& lightOff, int lightPin);

    /**
     * @brief Verifica a umidade atual contra o alvo e controla o umidificador.
     * @param currentHumidity Umidade atual (do SensorManager).
     * @param targetHumidity Umidade alvo (do TargetDataManager).
     * @param humidityPin Pino do umidificador.
     */
    void checkAndControlHumidity(float currentHumidity, float targetHumidity, int humidityPin);

    // --- Funções das Tarefas ---
    void runLightControlTask();
    void runHumidityControlTask();

    // --- Wrappers Estáticos para Tarefas ---
    static void lightControlTaskWrapper(void* pvParameters);
    static void humidityControlTaskWrapper(void* pvParameters);

    // --- Membros ---
    const GPIOControlConfig& gpioConfig; // Referência à config
    TargetDataManager& targetDataManager;  // Referência ao manager de alvos
    SensorManager& sensorManager;        // Referência ao manager de sensores
    int lastLightState = -1;             // Estado da luz (poderia ser booleano)
    TaskHandle_t lightTaskHandle = nullptr;
    TaskHandle_t humidityTaskHandle = nullptr;
    bool initialized = false;

    // Constantes internas
    static const TickType_t LIGHT_CHECK_INTERVAL_MS;
    static const TickType_t HUMIDITY_CHECK_INTERVAL_MS;
};

} // namespace GrowController

#endif // ACTUATOR_MANAGER_HPP