// src/ui/displayManager.hpp
#ifndef DISPLAY_MANAGER_HPP
#define DISPLAY_MANAGER_HPP

#include <stdint.h>
#include <memory>
#include <LiquidCrystal_I2C.h>
#include "utils/freeRTOSMutex.hpp"
#include "utils/timeService.hpp" // <<< ADICIONADO INCLUDE (VERIFIQUE CAMINHO)

namespace GrowController {

// Forward declare TimeService se não incluir o header diretamente
// class TimeService;

class DisplayManager {
public:
    /**
     * @brief Construtor. Recebe parâmetros de configuração e dependências.
     * @param i2c_addr Endereço I2C do display.
     * @param cols Número de colunas.
     * @param rows Número de linhas.
     * @param timeSvc Referência ao TimeService para obter a hora atual. // <<< NOVA DEP DOC
     */
    DisplayManager(uint8_t i2c_addr, uint8_t cols, uint8_t rows, TimeService& timeSvc); // <<< ADICIONADO timeSvc

    ~DisplayManager();

    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    bool initialize();
    void clear();
    void setBacklight(bool enable);
    void printLine(uint8_t line, const char* format, ...);
    void showError(const char* message);

    // Funções de Status Específico
    void showBooting();
    void showConnectingWiFi();
    void showWiFiConnected(const char* ip);
    void showNtpSyncing();
    void showNtpSynced(); // <<< USARÁ timeService
    void showMqttConnecting();
    void showMqttConnected();

    /**
     * @brief Atualiza a exibição principal com dados de sensores e hora. Thread-safe.
     * @param temp Temperatura em Celsius.
     * @param airHum Umidade do ar em %.
     * @param soilHum Umidade do solo em %.
     */
    void showSensorData(float temp, float airHum, float soilHum); // <<< USARÁ timeService

    void updateSpinner();
    bool isInitialized() const;

private:
    const uint8_t i2cAddr;
    const uint8_t lcdCols;
    const uint8_t lcdRows;
    TimeService& timeService;
    std::unique_ptr<LiquidCrystal_I2C> lcd = nullptr;
    FreeRTOSMutex lcdMutex;
    bool initialized = false;

    // Estado interno
    int8_t spinnerCounter = 0;
    float lastTemp = NAN;
    float lastAirHum = NAN;
    float lastSoilHum = NAN;
    int lastMinute = -1;

    static const char* SPINNER_GLYPHS;
    static const TickType_t MUTEX_TIMEOUT;
};

} // namespace GrowController

#endif // DISPLAY_MANAGER_HPP