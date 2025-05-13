#ifndef TARGET_DATA_MANAGER_HPP
#define TARGET_DATA_MANAGER_HPP

#include <ArduinoJson.h> // Para desserialização
#include <time.h>        // Para struct tm
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <Arduino.h> // Para Serial (logs de erro) e memset

namespace GrowController
{

  struct TargetValues
  {
    float airHumidity = 73;
    float vpd = NAN;
    float soilHumidity = NAN;
    float temperature = 25;
    struct tm lightOnTime;
    struct tm lightOffTime;
  };

  class TargetDataManager
  {
  public:
    /**
     * @brief Construtor. Inicializa os valores padrão e o mutex.
     */
    TargetDataManager();

    /**
     * @brief Destrutor. Libera o mutex.
     */
    ~TargetDataManager();

    // Desabilita cópia e atribuição para evitar problemas com o mutex
    TargetDataManager(const TargetDataManager &) = delete;
    TargetDataManager &operator=(const TargetDataManager &) = delete;

    /**
     * @brief Atualiza os valores alvo a partir de um JsonDocument.
     * Thread-safe. Analisa campos conhecidos e atualiza os valores internos.
     *
     * @param doc Referência constante ao JsonDocument contendo os novos valores.
     * @return true Se pelo menos um valor alvo foi atualizado com sucesso.
     * @return false Se o parsing falhou ou nenhum valor conhecido foi encontrado/atualizado.
     */
    bool updateTargetsFromJson(const JsonDocument &doc);

    /**
     * @brief Obtém uma cópia segura de todos os valores alvo atuais.
     * Thread-safe.
     *
     * @return TargetValues Uma cópia dos valores alvo. Retorna valores padrão/NAN se o mutex não puder ser obtido.
     */
    TargetValues getTargets() const;

    // --- Getters individuais (thread-safe) ---

    /**
     * @brief Obtém o valor alvo para a umidade do ar.
     * Thread-safe.
     * @return float Valor alvo em %, ou NAN se não definido ou erro ao obter mutex.
     */
    float getTargetAirHumidity() const;

    /**
     * @brief Obtém o horário alvo para ligar a luz.
     * Thread-safe.
     * @return struct tm Estrutura com a hora e minuto. Retorna {0} se erro ao obter mutex.
     */
    struct tm getLightOnTime() const;

    /**
     * @brief Obtém o horário alvo para desligar a luz.
     * Thread-safe.
     * @return struct tm Estrutura com a hora e minuto. Retorna {0} se erro ao obter mutex.
     */
    struct tm getLightOffTime() const;

    // Adicione getters para vpd, soilHumidity, temperature se forem necessários individualmente

  private:
    /**
     * @brief Helper interno para obter um valor float do JSON de forma segura.
     * @param key A chave JSON.
     * @param doc O JsonDocument.
     * @param outValue Referência para onde armazenar o valor.
     * @return true Se o valor foi encontrado, é um número e foi atualizado.
     * @return false Caso contrário.
     */
    bool _updateFloatValue(const char *key, const JsonDocument &doc, float &outValue);

    /**
     * @brief Helper interno para obter um valor de tempo (HH:MM) do JSON de forma segura.
     * @param key A chave JSON.
     * @param doc O JsonDocument.
     * @param outValue Referência para a struct tm a ser atualizada (apenas tm_hour, tm_min).
     * @return true Se o valor foi encontrado, é uma string no formato HH:MM e foi atualizado.
     * @return false Caso contrário.
     */
    bool _updateTimeValue(const char *key, const JsonDocument &doc, struct tm &outValue);

    TargetValues currentTargets;                              // Armazena os valores
    mutable SemaphoreHandle_t dataMutex;                      // Mutex para proteger currentTargets (mutable para getters const)
    static const TickType_t mutexTimeout = pdMS_TO_TICKS(200); // Timeout para leituras
  };

} // namespace GrowController
#endif // TARGET_DATA_MANAGER_HPP