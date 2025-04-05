#ifndef DISPLAY_MANAGER_HPP
#define DISPLAY_MANAGER_HPP

#include <stdint.h> // Para uint8_t

// --- Configuração ---
// Poderíamos ter uma struct DisplayConfig, mas por enquanto passamos diretamente.

// --- Funções de Gerenciamento ---

/**
 * @brief Inicializa o DisplayManager e o hardware LCD.
 * Deve ser chamado uma vez no setup(), após Wire.begin().
 *
 * @param i2c_addr Endereço I2C do display LCD.
 * @param cols Número de colunas do display.
 * @param rows Número de linhas do display.
 * @return true Se a inicialização foi bem-sucedida (mutex criado, LCD inicializado).
 * @return false Se a inicialização falhou.
 */
bool displayManagerInit(uint8_t i2c_addr, uint8_t cols, uint8_t rows);

/**
 * @brief Limpa completamente o display LCD.
 */
void displayManagerClear();

/**
 * @brief Controla a luz de fundo do LCD.
 *
 * @param enable true para ligar, false para desligar.
 */
void displayManagerSetBacklight(bool enable);

// --- Funções de Exibição de Conteúdo ---

/**
 * @brief Imprime texto em uma linha específica, limpando o restante da linha.
 * Usa snprintf internamente para segurança e formatação.
 *
 * @param line O número da linha (0 ou 1 para um display 16x2).
 * @param format Ponteiro para a string de formato (estilo printf).
 * @param ... Argumentos variáveis para a string de formato.
 */
void displayManagerPrintLine(uint8_t line, const char* format, ...);

/**
 * @brief Exibe uma mensagem de erro genérica (geralmente na linha inferior).
 *
 * @param message A mensagem de erro a ser exibida.
 */
void displayManagerShowError(const char* message);

// --- Funções de Exibição de Status Específico ---

/**
 * @brief Exibe a mensagem inicial de boot.
 */
void displayManagerShowBooting();

/**
 * @brief Exibe o status de conexão WiFi.
 */
void displayManagerShowConnectingWiFi();

/**
 * @brief Exibe o status de WiFi conectado com o IP.
 *
 * @param ip String contendo o endereço IP obtido.
 */
void displayManagerShowWiFiConnected(const char* ip);

/**
 * @brief Exibe o status de sincronização NTP.
 */
void displayManagerShowNtpSyncing();

/**
 * @brief Exibe o status de NTP sincronizado.
 */
void displayManagerShowNtpSynced();

/**
 * @brief Exibe o status de conexão MQTT.
 */
void displayManagerShowMqttConnecting();

/**
 * @brief Exibe o status de MQTT conectado.
 */
void displayManagerShowMqttConnected(); // Pode não ser necessário se a UI principal sobrescrever

/**
 * @brief Atualiza a exibição principal com os dados dos sensores.
 * Formata e exibe temperatura, umidade do ar e umidade do solo.
 *
 * @param temp Temperatura em graus Celsius.
 * @param airHum Umidade do ar em %.
 * @param soilHum Umidade do solo em %.
 */
void displayManagerShowSensorData(float temp, float airHum, float soilHum);

/**
 * @brief Atualiza o caractere do spinner em um local fixo (canto inferior direito).
 * Deve ser chamado periodicamente quando uma operação longa está em andamento.
 */
void displayManagerUpdateSpinner();


#endif // DISPLAY_MANAGER_HPP