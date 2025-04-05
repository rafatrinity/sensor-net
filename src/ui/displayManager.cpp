// displayManager.cpp

#include "displayManager.hpp"
#include "config.hpp"          // Apenas se precisar de algo daqui no futuro, idealmente não.
#include "utils/timeService.hpp" // <<< ADICIONADO: Para obter a hora atual
#include <time.h>              // <<< ADICIONADO: Para struct tm

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <stdarg.h> // Para va_list, va_start, va_end
#include <stdio.h>  // Para vsnprintf
#include <string.h> // Para strlen

// --- Variáveis Estáticas Internas (Encapsulamento) ---
static LiquidCrystal_I2C* lcd = nullptr;
static SemaphoreHandle_t lcdMutex = nullptr;
static uint8_t lcdCols = 0;
static uint8_t lcdRows = 0;
static bool displayInitialized = false;
static int8_t spinnerCounter = 0;
static const char* spinnerGlyphs = "|/-\\";
static const TickType_t mutexTimeout = pdMS_TO_TICKS(50);

// --- Implementação das Funções (init, clear, backlight, printLine, showError, status específico...) ---
// ... (O código anterior dessas funções permanece o mesmo) ...

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

    lcd = new LiquidCrystal_I2C(i2c_addr, cols, rows);
    if (lcd == nullptr) {
         Serial.println("DisplayManager ERROR: Failed to allocate LCD object!");
         vSemaphoreDelete(lcdMutex); // Limpa o mutex criado
         lcdMutex = nullptr;
         return false;
    }

    lcdCols = cols;
    lcdRows = rows;

    lcd->init();
    lcd->backlight();
    lcd->clear();

    displayInitialized = true;
    Serial.println("DisplayManager: Initialization successful.");
    return true;
}

void displayManagerClear() {
    if (!displayInitialized || lcd == nullptr) return;

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        lcd->clear();
        xSemaphoreGive(lcdMutex);
    } else {
        Serial.println("DisplayManager ERROR: Failed to acquire mutex for clear().");
    }
}

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

void displayManagerPrintLine(uint8_t line, const char* format, ...) {
    if (!displayInitialized || lcd == nullptr || line >= lcdRows) return;

    char buffer[lcdCols + 1];

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0) {
        Serial.println("DisplayManager ERROR: vsnprintf encoding error.");
        return;
    }
    len = (len >= (int)sizeof(buffer)) ? (sizeof(buffer) - 1) : len;
    buffer[len] = '\0';

    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        lcd->setCursor(0, line);
        lcd->print(buffer);

        for (int i = len; i < lcdCols; ++i) {
            lcd->print(" ");
        }
        xSemaphoreGive(lcdMutex);
    } else {
        Serial.println("DisplayManager ERROR: Failed to acquire mutex for printLine().");
    }
}

void displayManagerShowError(const char* message) {
     displayManagerPrintLine(lcdRows > 0 ? lcdRows - 1 : 0, "Error: %s", message);
}


// --- Implementação das Funções de Status Específico ---

void displayManagerShowBooting() {
    displayManagerPrintLine(0, "Booting...");
    displayManagerPrintLine(1, "");
}

void displayManagerShowConnectingWiFi() {
    displayManagerPrintLine(0, "Connecting WiFi");
    displayManagerPrintLine(1, ".");
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
    // Agora esta função pode opcionalmente mostrar a hora inicial,
    // mas displayManagerShowSensorData cuidará da atualização contínua.
    displayManagerPrintLine(0, "Time Synced");
    struct tm now;
    if (getCurrentTime(now)) { // Tenta obter a hora atual
        displayManagerPrintLine(1, "%02d:%02d", now.tm_hour, now.tm_min); // Mostra HH:MM na linha 1 temporariamente
    } else {
        displayManagerPrintLine(1, ""); // Limpa linha 1 se não conseguir a hora
    }
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
 * @brief Atualiza a exibição principal com os dados dos sensores e a hora atual.
 */
void displayManagerShowSensorData(float temp, float airHum, float soilHum) {
    if (!displayInitialized || lcd == nullptr) return;

    // Guarda os últimos valores para evitar atualizações desnecessárias
    static float lastTemp = -1000.0, lastAir = -1000.0, lastSoil = -1000.0;
    // Guarda o último minuto exibido para atualizar a hora apenas quando o minuto muda
    static int lastMinute = -1; // Inicializa com -1 para forçar a primeira atualização

    // Obtém a hora atual
    struct tm now;
    bool timeAvailable = getCurrentTime(now);

    // Verifica se houve mudanças
    // Tolerâncias pequenas para temp/hum para evitar flickering por ruído
    bool tempChanged = abs(temp - lastTemp) > 0.1 || isnan(temp) != isnan(lastTemp);
    bool humChanged = abs(airHum - lastAir) > 0.5 || abs(soilHum - lastSoil) > 0.5 || isnan(airHum) != isnan(lastAir) || isnan(soilHum) != isnan(lastSoil);
    bool minuteChanged = (timeAvailable && now.tm_min != lastMinute) || (!timeAvailable && lastMinute != -2); // Atualiza se o minuto mudar ou se a disponibilidade da hora mudar

    // Se nada mudou, retorna cedo
    if (!tempChanged && !humChanged && !minuteChanged) {
        return;
    }

    // Tenta pegar o mutex para acessar o LCD
    if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
        // --- Atualiza Linha 0 (Temperatura e Hora) ---
        if (tempChanged || minuteChanged) {
            lcd->setCursor(0, 0); // Coluna 0, Linha 0
            char line0Str[lcdCols + 1];
            char tempPart[10]; // Buffer para "T:XX.XC" ou "T: ERR"
            char timePart[10]; // Buffer para " H:HH:MM" ou " H:--:--"

            // Formata a parte da temperatura
            if (!isnan(temp)) {
                snprintf(tempPart, sizeof(tempPart), "T:%.1fC", temp);
            } else {
                snprintf(tempPart, sizeof(tempPart), "T: ERR");
            }

            // Formata a parte da hora
            if (timeAvailable) {
                snprintf(timePart, sizeof(timePart), " H:%02d:%02d", now.tm_hour, now.tm_min);
                lastMinute = now.tm_min; // Atualiza o último minuto exibido
            } else {
                snprintf(timePart, sizeof(timePart), " H:--:--");
                lastMinute = -2; // Marca que a hora não estava disponível
            }

            // Combina as duas partes, garantindo que caiba
            snprintf(line0Str, sizeof(line0Str), "%s%s", tempPart, timePart);

            // Imprime e limpa o resto da linha
            lcd->print(line0Str);
            for(int i = strlen(line0Str); i < lcdCols; ++i) {
                 lcd->print(" ");
            }

            // Atualiza o último valor de temperatura
            lastTemp = temp;
        }

        // --- Atualiza Linha 1 (Umidades) ---
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
                 // Garante que não estoure o buffer antes de escrever a umidade do solo
                 if (sizeof(humStr) - written > 6) { // "Sol:XXX%" precisa de ~6+ chars
                    written += snprintf(humStr + written, sizeof(humStr) - written, "Sol:%.0f%%", soilHum);
                 }
             } else {
                 // Garante que não estoure o buffer
                 if (sizeof(humStr) - written > 7) { // "Sol:ERR"
                    written += snprintf(humStr + written, sizeof(humStr) - written, "Sol:ERR");
                 }
             }

            // Imprime e limpa o resto da linha
            lcd->print(humStr);
            for(int i = written; i < lcdCols; ++i) {
                lcd->print(" ");
            }

            // Atualiza os últimos valores de umidade
            lastAir = airHum;
            lastSoil = soilHum;
        }

        // Libera o mutex
        xSemaphoreGive(lcdMutex);

    } else {
         // Evita floodar o serial se o mutex estiver ocupado frequentemente
         // Serial.println("DisplayManager WARN: Failed to acquire mutex for showSensorData().");
    }
}


/**
 * @brief Atualiza o caractere do spinner em um local fixo (canto inferior direito).
 */
void displayManagerUpdateSpinner() {
     if (!displayInitialized || lcd == nullptr) return;

     spinnerCounter = (spinnerCounter + 1) % strlen(spinnerGlyphs);

     if (xSemaphoreTake(lcdMutex, mutexTimeout) == pdTRUE) {
         lcd->setCursor(lcdCols > 0 ? lcdCols - 1 : 0, lcdRows > 0 ? lcdRows - 1 : 0);
         lcd->print(spinnerGlyphs[spinnerCounter]);
         xSemaphoreGive(lcdMutex);
     } else {
        // Serial.println("DisplayManager WARN: Failed to acquire mutex for updateSpinner().");
     }
}