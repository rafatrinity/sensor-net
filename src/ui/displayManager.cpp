// src/ui/displayManager.cpp
#include "displayManager.hpp"
#include "utils/timeService.hpp" // Para getCurrentTime
#include <time.h>
#include <Arduino.h>           // Para Serial, etc.
#include <stdarg.h>            // Para va_list
#include <stdio.h>             // Para vsnprintf
#include <string.h>            // Para strlen, abs
#include <math.h>              // Para isnan

namespace GrowController {

// Definição das constantes estáticas
const char* DisplayManager::SPINNER_GLYPHS = "|/-\\";
const TickType_t DisplayManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(100);

// --- Construtor e Destrutor ---

DisplayManager::DisplayManager(uint8_t i2c_addr, uint8_t cols, uint8_t rows) :
    i2cAddr(i2c_addr),
    lcdCols(cols),
    lcdRows(rows),
    lcd(nullptr), // Inicializa unique_ptr como null
    // lcdMutex é inicializado automaticamente pelo seu construtor
    initialized(false),
    spinnerCounter(0),
    lastTemp(NAN), // Usar NAN para estado inicial inválido
    lastAirHum(NAN),
    lastSoilHum(NAN),
    lastMinute(-1)
{}

// Destrutor pode ser vazio, RAII cuida da limpeza do lcd e do lcdMutex.
DisplayManager::~DisplayManager() {
    // unique_ptr<LiquidCrystal_I2C> deletará o objeto lcd automaticamente.
    // FreeRTOSMutex deletará o mutex automaticamente.
    Serial.println("DisplayManager: Destroyed."); // Log opcional
}

// --- Inicialização ---

bool DisplayManager::initialize() {
    if (initialized) {
        Serial.println("DisplayManager WARN: Already initialized.");
        return true;
    }

    Serial.print("DisplayManager: Initializing LCD (Addr: 0x");
    Serial.print(i2cAddr, HEX); Serial.print(", ");
    Serial.print(lcdCols); Serial.print("x"); Serial.print(lcdRows);
    Serial.println(")...");

    // 1. Verifica se o Mutex foi criado com sucesso (pelo construtor do FreeRTOSMutex)
    if (!lcdMutex) { // Usa o operator bool() da classe FreeRTOSMutex
        Serial.println("DisplayManager ERROR: Failed to create mutex!");
        return false;
    }

    // 2. Aloca e inicializa o LCD usando unique_ptr
    // Usar reset() ou make_unique (se disponível e preferido)
    // lcd = std::make_unique<LiquidCrystal_I2C>(i2cAddr, lcdCols, lcdRows);
    lcd.reset(new (std::nothrow) LiquidCrystal_I2C(i2cAddr, lcdCols, lcdRows)); // Use nothrow para evitar exceção

    if (!lcd) {
        Serial.println("DisplayManager ERROR: Failed to allocate LCD object!");
        // Não precisa deletar mutex, RAII cuida disso se initialize falhar
        return false;
    }

    // 3. Inicializa o hardware do LCD
    // Adicionar verificação se init() tem retorno ou lança exceção (improvável aqui)
    lcd->init();
    lcd->backlight(); // Liga backlight por padrão
    lcd->clear();

    initialized = true;
    Serial.println("DisplayManager: Initialization successful.");
    return true;
}

bool DisplayManager::isInitialized() const {
    return initialized;
}

// --- Métodos de Controle ---

void DisplayManager::clear() {
    if (!initialized || !lcd) return;

    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        lcd->clear();
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager ERROR: Mutex timeout on clear().");
    }
}

void DisplayManager::setBacklight(bool enable) {
    if (!initialized || !lcd) return;

    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        if (enable) {
            lcd->backlight();
        } else {
            lcd->noBacklight();
        }
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager ERROR: Mutex timeout on setBacklight().");
    }
}

// --- Métodos de Impressão ---

void DisplayManager::printLine(uint8_t line, const char* format, ...) {
    if (!initialized || !lcd || line >= lcdRows) return;

    char buffer[lcdCols + 1]; // Buffer local na stack

    // Formatação segura com vsnprintf
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Verifica erro de codificação ou buffer pequeno (vsnprintf retorna o tamanho necessário)
    if (len < 0) {
        Serial.println("DisplayManager ERROR: vsnprintf encoding error.");
        return; // Sai da função se houver erro
    }
    // Trunca se necessário (embora vsnprintf já faça isso)
    // len = (len >= (int)sizeof(buffer)) ? (sizeof(buffer) - 1) : len;
    // buffer[len] = '\0'; // vsnprintf já garante terminação nula se couber

    // Acesso thread-safe ao LCD
    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        lcd->setCursor(0, line);
        lcd->print(buffer);

        // Limpa o restante da linha
        // Usa a 'len' retornada por vsnprintf que é o número de caracteres *escritos* (sem o nulo)
        // Mas cuidado: se len >= lcdCols, não precisa limpar nada.
        int charsToPrint = strlen(buffer); // strlen pode ser mais seguro aqui
        for (int i = charsToPrint; i < lcdCols; ++i) {
            lcd->print(" ");
        }
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager ERROR: Mutex timeout on printLine().");
    }
}

void DisplayManager::showError(const char* message) {
    // Imprime na última linha disponível
    uint8_t errorLine = (lcdRows > 0) ? lcdRows - 1 : 0;
    printLine(errorLine, "Error: %s", message ? message : "Unknown");
}

// --- Métodos de Status Específico ---

void DisplayManager::showBooting() {
    printLine(0, "Booting...");
    if (lcdRows > 1) printLine(1, "");
}

void DisplayManager::showConnectingWiFi() {
    printLine(0, "Connecting WiFi");
    if (lcdRows > 1) printLine(1, "."); // Spinner inicial
}

void DisplayManager::showWiFiConnected(const char* ip) {
    printLine(0, "WiFi Connected!");
    if (lcdRows > 1) printLine(1, "IP: %s", ip ? ip : "?.?.?.?");
}

void DisplayManager::showNtpSyncing() {
    printLine(0, "Syncing Time...");
    if (lcdRows > 1) printLine(1, "");
}

void DisplayManager::showNtpSynced() {
    printLine(0, "Time Synced");
    struct tm now = {0}; // Inicializa struct tm
    if (lcdRows > 1) {
        if (getCurrentTime(now)) {
            printLine(1, "%02d:%02d", now.tm_hour, now.tm_min);
        } else {
            printLine(1, "--:--"); // Indica que hora não está disponível
        }
    }
}

void DisplayManager::showMqttConnecting() {
    printLine(0, "MQTT Connecting..");
    if (lcdRows > 1) printLine(1, "");
}

void DisplayManager::showMqttConnected() {
    printLine(0, "MQTT Connected");
    if (lcdRows > 1) printLine(1, "");
}

// --- Atualização Principal ---

void DisplayManager::showSensorData(float temp, float airHum, float soilHum) {
    if (!initialized || !lcd) return;

    // Obter hora atual
    struct tm now = {0};
    bool timeAvailable = getCurrentTime(now);

    // Verificar mudanças (lógica similar à anterior, mas usando membros da classe)
    // Usar uma pequena tolerância para floats para evitar flickering
    bool tempChanged = isnan(temp) != isnan(this->lastTemp) || (!isnan(temp) && abs(temp - this->lastTemp) > 0.1f);
    bool airHumChanged = isnan(airHum) != isnan(this->lastAirHum) || (!isnan(airHum) && abs(airHum - this->lastAirHum) > 0.5f);
    bool soilHumChanged = isnan(soilHum) != isnan(this->lastSoilHum) || (!isnan(soilHum) && abs(soilHum - this->lastSoilHum) > 0.5f);
    bool humChanged = airHumChanged || soilHumChanged; // Combina umidades
    bool minuteChanged = (timeAvailable && now.tm_min != this->lastMinute) || (timeAvailable != (this->lastMinute != -2)); // Atualiza se minuto ou disponibilidade da hora muda


    // Se nada significativo mudou, sair cedo
    if (!tempChanged && !humChanged && !minuteChanged) {
        return;
    }

    // Tentar pegar o mutex
    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {

        // --- Atualiza Linha 0 (Temperatura e Hora) ---
        if (tempChanged || minuteChanged) {
            char line0Str[lcdCols + 1] = {0}; // Buffer para linha 0
            char tempPart[12] = {0};          // "T:XXX.XC" ou "T:ERR"
            char timePart[10] = {0};          // " H:HH:MM" ou " H:--:--"

            // Formata Temperatura
            if (!isnan(temp)) {
                snprintf(tempPart, sizeof(tempPart), "T:%.1fC", temp);
                this->lastTemp = temp; // Atualiza cache interno
            } else {
                snprintf(tempPart, sizeof(tempPart), "T:ERR");
                 this->lastTemp = NAN;
            }

            // Formata Hora
            if (timeAvailable) {
                snprintf(timePart, sizeof(timePart), " H:%02d:%02d", now.tm_hour, now.tm_min);
                this->lastMinute = now.tm_min;
            } else {
                snprintf(timePart, sizeof(timePart), " H:--:--");
                this->lastMinute = -2; // Marca que hora não estava disponível
            }

            // Combina e imprime
            snprintf(line0Str, sizeof(line0Str), "%s%s", tempPart, timePart);
            lcd->setCursor(0, 0);
            lcd->print(line0Str);
            // Limpa resto da linha
            for (int i = strlen(line0Str); i < lcdCols; ++i) lcd->print(" ");
        }

        // --- Atualiza Linha 1 (Umidades) ---
        if (humChanged && lcdRows > 1) { // Só atualiza se houver linha 1
            char line1Str[lcdCols + 1] = {0};
            char airPart[12] = {0};  // "Air:XXX%" ou "Air:ERR "
            char soilPart[12] = {0}; // "Sol:XXX%" ou "Sol:ERR"

            // Formata Umidade Ar
            if (!isnan(airHum)) {
                snprintf(airPart, sizeof(airPart), "Air:%.0f%% ", airHum);
                this->lastAirHum = airHum;
            } else {
                snprintf(airPart, sizeof(airPart), "Air:ERR ");
                this->lastAirHum = NAN;
            }

            // Formata Umidade Solo
            if (!isnan(soilHum)) {
                snprintf(soilPart, sizeof(soilPart), "Sol:%.0f%%", soilHum);
                this->lastSoilHum = soilHum;
            } else {
                 snprintf(soilPart, sizeof(soilPart), "Sol:ERR");
                this->lastSoilHum = NAN;
            }

            // Combina e imprime
            snprintf(line1Str, sizeof(line1Str), "%s%s", airPart, soilPart);
            lcd->setCursor(0, 1);
            lcd->print(line1Str);
            // Limpa resto da linha
            for (int i = strlen(line1Str); i < lcdCols; ++i) lcd->print(" ");
        }

        // Libera mutex
        xSemaphoreGive(lcdMutex.get());

    } else {
        // Log apenas se a falha persistir? Evitar flood.
        // static uint32_t failCount = 0; if (++failCount % 10 == 0) ...
        Serial.println("DisplayManager WARN: Mutex timeout on showSensorData().");
    }
}


void DisplayManager::updateSpinner() {
    if (!initialized || !lcd) return;

    // Atualiza contador do spinner
    this->spinnerCounter = (this->spinnerCounter + 1) % strlen(SPINNER_GLYPHS);

    // Tenta pegar o mutex
    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        // Posiciona no canto inferior direito
        uint8_t spinnerCol = (lcdCols > 0) ? lcdCols - 1 : 0;
        uint8_t spinnerRow = (lcdRows > 0) ? lcdRows - 1 : 0;
        lcd->setCursor(spinnerCol, spinnerRow);
        lcd->print(SPINNER_GLYPHS[this->spinnerCounter]); // Usa membro da classe
        xSemaphoreGive(lcdMutex.get());
    } else {
       // Serial.println("DisplayManager WARN: Mutex timeout on updateSpinner().");
    }
}

} // namespace GrowController