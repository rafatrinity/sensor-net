// src/sensors/sensorManager.hpp
#ifndef SENSOR_MANAGER_HPP
#define SENSOR_MANAGER_HPP

#include "config.hpp"             // Para SensorConfig
#include <memory>                // Para std::unique_ptr
#include <DHT.h>                 // Biblioteca DHT
#include <vector>                // Para leitura do sensor de solo
#include <numeric>               // Para std::accumulate
#include "utils/freeRTOSMutex.hpp" // Wrapper RAII do Mutex
#include "freertos/FreeRTOS.h"   // Para tipos FreeRTOS
#include "freertos/task.h"       // Para TaskHandle_t
#include "utils/timeService.hpp"

// Forward declaration para dependências
namespace GrowController {
    class DisplayManager;
    class MqttManager;
    class DataHistoryManager;
}

namespace GrowController {

/**
 * @brief Gerencia a leitura de sensores (DHT, Solo), cálculo de VPD,
 *        e armazena os resultados em cache.
 * Atualiza um cache interno thread-safe e executa uma tarefa dedicada
 * para leituras periódicas. Utiliza RAII para recursos.
 */
class SensorManager {
public:
    /**
     * @brief Construtor. Recebe configuração e dependências.
     * @param config Configuração específica dos sensores.
     * @param timeSvc Serviço de tempo (para leitura de tempo).
     * @param historyMgr Ponteiro para o DataHistoryManager (opcional, para armazenamento em cache).
     * @param displayMgr Ponteiro para o DisplayManager (opcional, para atualização direta).
     * @param mqttMgr Ponteiro para o MqttManager (opcional, para publicação direta).
     */
    SensorManager(const SensorConfig& config,
                  TimeService& timeSvc,
                  DataHistoryManager* historyMgr,
                  DisplayManager* displayMgr = nullptr,
                  MqttManager* mqttMgr = nullptr);

    /**
     * @brief Destrutor. Para a tarefa e libera recursos (automático via RAII).
     */
    ~SensorManager();

    // Desabilitar cópia e atribuição
    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    /**
     * @brief Inicializa o hardware do sensor DHT e o mutex.
     * @return true Se a inicialização foi bem-sucedida.
     * @return false Se houve falha.
     */
    bool initialize();

    /**
     * @brief Inicia a tarefa FreeRTOS para leitura periódica dos sensores.
     * @param priority Prioridade da tarefa.
     * @param stackSize Tamanho da pilha da tarefa em palavras (words).
     * @return true Se a tarefa foi criada com sucesso.
     * @return false Se a criação da tarefa falhou.
     */
    bool startSensorTask(UBaseType_t priority = 1, uint32_t stackSize = 4096);

    /**
     * @brief Obtém a última leitura de temperatura válida do cache. Thread-safe.
     * @return float Temperatura em Celsius ou NAN.
     */
    float getTemperature() const;

    /**
     * @brief Obtém a última leitura de umidade do ar válida do cache. Thread-safe.
     * @return float Umidade em % ou NAN.
     */
    float getHumidity() const;

    /**
     * @brief Obtém a última leitura de umidade do solo válida do cache. Thread-safe.
     * @return float Umidade do solo em % ou NAN.
     */
    float getSoilHumidity() const; // NOVO GETTER

    /**
     * @brief Obtém o último valor calculado de VPD do cache. Thread-safe.
     * @return float VPD em kPa ou NAN.
     */
    float getVpd() const; // NOVO GETTER

    /**
     * @brief Verifica se o manager foi inicializado.
     * @return true se inicializado, false caso contrário.
     */
    bool isInitialized() const;


private:
    /**
     * @brief Função executada pelo loop da tarefa FreeRTOS.
     */
    void runSensorTask();

    /**
     * @brief Realiza a leitura direta do sensor de temperatura DHT. Não thread-safe.
     * @return float Temperatura em Celsius ou NAN.
     */
    float _readTemperatureFromSensor();

    /**
     * @brief Realiza a leitura direta do sensor de umidade DHT. Não thread-safe.
     * @return float Umidade em % ou NAN.
     */
    float _readHumidityFromSensor();

     /**
     * @brief Lê a umidade do solo diretamente do pino ADC configurado.
     * Esta função realiza a leitura no momento da chamada. Não thread-safe por si só,
     * mas chamada apenas pela tarefa interna.
     * @return float Umidade do solo em % ou NAN.
     */
    float _readSoilHumidityFromSensor() const; // MOVIDO PARA PRIVATE, RENOMEADO

    /**
     * @brief Calcula o Déficit de Pressão de Vapor (VPD).
     * @param temp Temperatura em Celsius.
     * @param hum Umidade do ar em %.
     * @return float VPD em kPa ou NAN se entradas inválidas.
     */
    float _calculateVpd(float temp, float hum); // MOVIDO PARA PRIVATE, RENOMEADO, NÃO ESTÁTICO

    /**
     * @brief Wrapper estático para a função da tarefa FreeRTOS.
     * @param pvParameters Ponteiro para a instância de SensorManager.
     */
    static void readSensorsTaskWrapper(void *pvParameters);

    // --- Membros ---
    const SensorConfig& sensorConfig;
    TimeService& timeServiceRef;
    DataHistoryManager* dataHistoryManagerPtr;
    DisplayManager* displayManager = nullptr;
    MqttManager* mqttManager = nullptr;
    std::unique_ptr<DHT> dhtSensor = nullptr;
    FreeRTOSMutex sensorDataMutex;
    float cachedTemperature = NAN;
    float cachedHumidity = NAN;
    float cachedSoilHumidity = NAN;
    float cachedVpd = NAN;
    TaskHandle_t readTaskHandle = nullptr;
    bool initialized = false;

     // Membros para acumulação de médias
    float currentTempSum = 0.0f;
    int validTempReadings = 0;
    float currentAirHumSum = 0.0f;
    int validAirHumReadings = 0;
    float currentSoilHumSum = 0.0f;
    int validSoilHumReadings = 0;

    unsigned long lastSaveToFlashMillis = 0;
    static const unsigned long SAVE_INTERVAL_MILLIS = 30UL * 60UL * 1000UL;

    // Constantes internas
    static const TickType_t SENSOR_READ_INTERVAL_MS;
    static const TickType_t MUTEX_TIMEOUT;
};

} // namespace GrowController

#endif // SENSOR_MANAGER_HPP