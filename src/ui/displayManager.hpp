// src/ui/displayManager.hpp
#ifndef DISPLAY_MANAGER_HPP
#define DISPLAY_MANAGER_HPP

#include <stdint.h>           // Para uint8_t
#include <memory>             // Para std::unique_ptr
#include <LiquidCrystal_I2C.h> // Biblioteca do LCD
#include "utils/freeRTOSMutex.hpp" // Nosso wrapper RAII para Mutex

namespace GrowController {

/**
 * @brief Gerencia a interação com um display LCD I2C de forma thread-safe.
 * Utiliza RAII para gerenciar o objeto LCD e o Mutex.
 */
class DisplayManager {
public:
    /**
     * @brief Construtor. Apenas armazena parâmetros de configuração.
     * A inicialização real do hardware é feita em initialize().
     * @param i2c_addr Endereço I2C do display.
     * @param cols Número de colunas.
     * @param rows Número de linhas.
     */
    DisplayManager(uint8_t i2c_addr, uint8_t cols, uint8_t rows);

    /**
     * @brief Destrutor. Garante a liberação de recursos (automática via RAII).
     */
    ~DisplayManager();

    // Desabilitar cópia e atribuição
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    /**
     * @brief Inicializa o hardware do LCD e o mutex.
     * Deve ser chamado uma vez após Wire.begin().
     * @return true Se a inicialização foi bem-sucedida.
     * @return false Se houve falha (ex: mutex não criado, LCD não alocado/inicializado).
     */
    bool initialize();

    /**
     * @brief Limpa completamente o display LCD. Thread-safe.
     */
    void clear();

    /**
     * @brief Controla a luz de fundo do LCD. Thread-safe.
     * @param enable true para ligar, false para desligar.
     */
    void setBacklight(bool enable);

    /**
     * @brief Imprime texto formatado em uma linha específica, limpando o restante. Thread-safe.
     * @param line O número da linha (base 0).
     * @param format String de formato estilo printf.
     * @param ... Argumentos variáveis.
     */
    void printLine(uint8_t line, const char* format, ...);

    /**
     * @brief Exibe uma mensagem de erro (geralmente na última linha). Thread-safe.
     * @param message Mensagem de erro.
     */
    void showError(const char* message);

    // --- Funções de Status Específico ---
    void showBooting();
    void showConnectingWiFi();
    void showWiFiConnected(const char* ip);
    void showNtpSyncing();
    void showNtpSynced();
    void showMqttConnecting();
    void showMqttConnected();

    /**
     * @brief Atualiza a exibição principal com dados de sensores e hora. Thread-safe.
     * @param temp Temperatura em Celsius.
     * @param airHum Umidade do ar em %.
     * @param soilHum Umidade do solo em %.
     */
    void showSensorData(float temp, float airHum, float soilHum);

    /**
     * @brief Atualiza o caractere do spinner no canto inferior direito. Thread-safe.
     */
    void updateSpinner();

    /**
     * @brief Verifica se o display manager foi inicializado com sucesso.
     * @return true se inicializado, false caso contrário.
     */
    bool isInitialized() const;

private:
    const uint8_t i2cAddr; // Guardar endereço I2C
    const uint8_t lcdCols;
    const uint8_t lcdRows;
    std::unique_ptr<LiquidCrystal_I2C> lcd = nullptr; // Ponteiro inteligente para o LCD
    FreeRTOSMutex lcdMutex; // Wrapper RAII para o Mutex
    bool initialized = false;

    // Estado interno para otimizar atualizações do display
    int8_t spinnerCounter = 0;
    float lastTemp = NAN;
    float lastAirHum = NAN;
    float lastSoilHum = NAN;
    int lastMinute = -1;

    // Constantes internas
    static const char* SPINNER_GLYPHS;
    static const TickType_t MUTEX_TIMEOUT;
};

} // namespace GrowController

#endif // DISPLAY_MANAGER_HPP