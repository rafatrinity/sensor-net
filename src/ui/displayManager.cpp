#include "displayManager.hpp"
#include "config.hpp" // Apenas se precisar de algo daqui no futuro, idealmente não.

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <stdarg.h> // Para va_list, va_start, va_end
#include <stdio.h>  // Para vsnprintf
#include <string.h> // Para strlen (usado pelo spinner antigo)

// --- Variáveis Estáticas Internas (Encapsulamento) ---

static LiquidCrystal_I2C* lcd = nullptr; // Ponteiro para o objeto LCD
static SemaphoreHandle_t lcdMutex = nullptr;
static uint8_t lcdCols = 0;
static uint8_t lcdRows = 0;
static bool displayInitialized = false;

// Estado interno do spinner
static int8_t spinnerCounter = 0;
static const char* spinnerGlyphs = "|/-\\"; // Glyphs mais simples e comuns

// Tempo para esperar pelos mutexes (em Ticks)
static const TickType_t mutexTimeout = pdMS_TO_TICKS(50); // 50 ms

// --- Implementação das Funções ---

/**
 * @brief Inicializa o DisplayManager e o hardware LCD.
 */
bool displayManagerInit(uint8_t i2c_addr, uint8_t cols, uint8_t rows) {
    if (displayInitialized) {
        Serial.println("DisplayManager WARN: Already initialized.");
        return true;
    }

    Serial.print("DisplayManager: Initializing LCD (Addr: 0x");
    Serial.print(i2c_addr, HEX);
    Serial.print(", ");
    Serial.print(cols);
    Serial.print("x");
    Serial.print(rows);
    Serial.println(")...");

    lcdMutex = xSemaphoreCreateMutex();
    if (lcdMutex == nullptr) {
        Serial.println("DisplayManager ERROR: Failed to create mutex!");
        return false;
    }

    // Aloca dinamicamente o objeto LCD
    // Isso evita problemas de ordem de inicialização de construtores globais
    // se LCD fosse static LiquidCrystal_I2C(...);
    lcd = new LiquidCrystal_I2C(i2c_addr, cols, rows);
    if (lcd == nullptr) {
         Serial.println("DisplayManager ERROR: Failed to allocate LCD object!");
         vSemaphoreDelete(lcdMutex); // Limpa o mutex criado
         lcdMutex = nullptr;
         return false;
    }

    // Armazena dimensões
    lcdCols = cols;
    lcdRows = rows;

    // Tenta inicializar o hardware
    // O próprio objeto LiquidCrystal_I2C chama Wire.begin() se não iniciado,
    // mas é boa prática chamar antes em main.ino.
    lcd->init();
    lcd->backlight(); // Liga a luz de fundo por padrão
    lcd->clear();

    displayInitialized = true;
    Serial.println("DisplayManager: Initialization successful.");
    return true;
}

/**
 * @brief Limpa completamente o display LCD.
 */
void displayManagerClear() {
    if (!displayInitialized || lcd == nullptr) return;

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        lcd->clear();
        xSemaphoreGive(lcdMutex);
    } else {
        Serial.println("DisplayManager ERROR: Failed to acquire mutex for clear().");
    }
}

/**
 * @brief Controla a luz de fundo do LCD.
 */
void displayManagerSetBacklight(bool enable) {
     if (!displayInitialized || lcd == nullptr) return;

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        if (enable) {
            lcd->backlight();
        } else {
            lcd->noBacklight();
        }
        xSemaphoreGive(lcdMutex);
    } else {
        Serial.println("DisplayManager ERROR: Failed to acquire mutex for setBacklight().");
    }
}

/**
 * @brief Imprime texto em uma linha específica, limpando o restante da linha.
 */
void displayManagerPrintLine(uint8_t line, const char* format, ...) {
    if (!displayInitialized || lcd == nullptr || line >= lcdRows) return;

    char buffer[lcdCols + 1]; // Buffer para a linha + null terminator

    // Formata a string usando varargs
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Garante que não excedemos o buffer (vsnprintf retorna o número de caracteres *que seriam* escritos)
    if (len < 0) {
        Serial.println("DisplayManager ERROR: vsnprintf encoding error.");
        return; // Erro de codificação
    }
    // Trunca se necessário (vsnprintf já faz isso implicitamente no buffer, mas len pode ser maior)
    len = (len >= (int)sizeof(buffer)) ? (sizeof(buffer) - 1) : len;
    buffer[len] = '\0'; // Garante terminação nula

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        lcd->setCursor(0, line);
        lcd->print(buffer);

        // Limpa o restante da linha
        for (int i = len; i < lcdCols; ++i) {
            lcd->print(" ");
        }
        xSemaphoreGive(lcdMutex);
    } else {
        Serial.println("DisplayManager ERROR: Failed to acquire mutex for printLine().");
    }
}

/**
 * @brief Exibe uma mensagem de erro genérica (geralmente na linha inferior).
 */
void displayManagerShowError(const char* message) {
     // Exibe na última linha por padrão
     displayManagerPrintLine(lcdRows > 0 ? lcdRows - 1 : 0, "Error: %s", message);
}


// --- Implementação das Funções de Status Específico ---

void displayManagerShowBooting() {
    displayManagerPrintLine(0, "Booting...");
    displayManagerPrintLine(1, ""); // Limpa linha 1
}

void displayManagerShowConnectingWiFi() {
    displayManagerPrintLine(0, "Connecting WiFi");
    // Linha 1 pode mostrar o spinner ou ficar limpa
    displayManagerPrintLine(1, "."); // Adiciona pontos progressivamente se chamado em loop
}

void displayManagerShowWiFiConnected(const char* ip) {
    displayManagerPrintLine(0, "WiFi Connected!");
    displayManagerPrintLine(1, "IP: %s", ip ? ip : "?.?.?.?");
}

void displayManagerShowNtpSyncing() {
    displayManagerPrintLine(0, "Syncing Time...");
    displayManagerPrintLine(1, "");
}

void displayManagerShowNtpSynced() {
    displayManagerPrintLine(0, "Time Synced");
    displayManagerPrintLine(1, ""); // Limpa linha 1, ou pode mostrar a hora aqui
    // Exemplo mostrando a hora (precisaria da função getTime do TimeService)
    // struct tm now;
    // if (getCurrentTime(now)) { // Assumindo que getCurrentTime está acessível
    //     char timeStr[9];
    //     snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
    //     displayManagerPrintLine(1, timeStr);
    // }
}

void displayManagerShowMqttConnecting() {
    displayManagerPrintLine(0, "MQTT Connecting..");
    displayManagerPrintLine(1, "");
}

void displayManagerShowMqttConnected() {
     displayManagerPrintLine(0, "MQTT Connected");
     displayManagerPrintLine(1, "");
}


/**
 * @brief Atualiza a exibição principal com os dados dos sensores.
 */
void displayManagerShowSensorData(float temp, float airHum, float soilHum) {
    // Guarda os últimos valores para evitar atualizações desnecessárias
    // (pode mover para variáveis static fora da função se preferir)
    static float lastTemp = -1000.0, lastAir = -1000.0, lastSoil = -1000.0;
    bool tempChanged = abs(temp - lastTemp) > 0.05 || isnan(temp) != isnan(lastTemp);
    bool humChanged = abs(airHum - lastAir) > 0.1 || abs(soilHum - lastSoil) > 0.1 || isnan(airHum) != isnan(lastAir) || isnan(soilHum) != isnan(lastSoil);

    if (!tempChanged && !humChanged) {
        return; // Nada mudou significativamente
    }

    if (!displayInitialized || lcd == nullptr) return;

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        // Atualiza Linha 0 (Temperatura) se necessário
        if (tempChanged) {
            lcd->setCursor(0, 0); // Coluna 0, Linha 0
            char tempStr[lcdCols + 1];
            if (!isnan(temp)) {
                snprintf(tempStr, sizeof(tempStr), "Tmp:%.1fC", temp);
            } else {
                snprintf(tempStr, sizeof(tempStr), "Tmp: ERR");
            }
            lcd->print(tempStr);
             // Limpa o resto
            for(int i = strlen(tempStr); i < lcdCols; ++i) lcd->print(" ");
            lastTemp = temp; // Atualiza último valor
        }

        // Atualiza Linha 1 (Umidades) se necessário
        if (humChanged) {
            lcd->setCursor(0, 1); // Coluna 0, Linha 1
            char humStr[lcdCols + 1];
            int written = 0;
            if (!isnan(airHum)) {
                 written += snprintf(humStr + written, sizeof(humStr) - written, "Air:%.0f%% ", airHum);
            } else {
                 written += snprintf(humStr + written, sizeof(humStr) - written, "Air:ERR ");
            }
             if (!isnan(soilHum)) {
                 written += snprintf(humStr + written, sizeof(humStr) - written, "Sol:%.0f%%", soilHum);
             } else {
                 written += snprintf(humStr + written, sizeof(humStr) - written, "Sol:ERR");
             }
            lcd->print(humStr);
             // Limpa o resto
             for(int i = written; i < lcdCols; ++i) lcd->print(" ");

            lastAir = airHum; // Atualiza últimos valores
            lastSoil = soilHum;
        }
        xSemaphoreGive(lcdMutex);
    } else {
         Serial.println("DisplayManager ERROR: Failed to acquire mutex for showSensorData().");
    }
}

/**
 * @brief Atualiza o caractere do spinner em um local fixo (canto inferior direito).
 */
void displayManagerUpdateSpinner() {
     if (!displayInitialized || lcd == nullptr) return;

     // Calcula o índice do próximo caractere
     spinnerCounter = (spinnerCounter + 1) % strlen(spinnerGlyphs);

     if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
         // Posiciona no último caractere da última linha
         lcd->setCursor(lcdCols > 0 ? lcdCols - 1 : 0, lcdRows > 0 ? lcdRows - 1 : 0);
         lcd->print(spinnerGlyphs[spinnerCounter]); // Imprime o caractere atual
         xSemaphoreGive(lcdMutex);
     } else {
        // Não loga erro aqui para não poluir o serial durante operações longas
        // Serial.println("DisplayManager WARN: Failed to acquire mutex for updateSpinner().");
     }
}