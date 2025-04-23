// src/ui/displayManager.cpp
#include "displayManager.hpp"
// #include "utils/timeService.hpp" // <<< REMOVIDO - Incluído via displayManager.hpp
#include <time.h>
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace GrowController {

const char* DisplayManager::SPINNER_GLYPHS = "|/-\\";
const TickType_t DisplayManager::MUTEX_TIMEOUT = pdMS_TO_TICKS(100);

// --- Construtor e Destrutor ---

// <<< CONSTRUTOR ATUALIZADO para receber e inicializar timeService
DisplayManager::DisplayManager(uint8_t i2c_addr, uint8_t cols, uint8_t rows, TimeService& timeSvc) :
    i2cAddr(i2c_addr),
    lcdCols(cols),
    lcdRows(rows),
    timeService(timeSvc), // <<< INICIALIZA o membro timeService
    lcd(nullptr),
    initialized(false),
    spinnerCounter(0),
    lastTemp(NAN),
    lastAirHum(NAN),
    lastSoilHum(NAN),
    lastMinute(-1)
{}

DisplayManager::~DisplayManager() {
    Serial.println("DisplayManager: Destroyed.");
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

    if (!lcdMutex) {
        Serial.println("DisplayManager ERROR: Failed to create mutex!");
        return false;
    }

    lcd.reset(new (std::nothrow) LiquidCrystal_I2C(i2cAddr, lcdCols, lcdRows));

    if (!lcd) {
        Serial.println("DisplayManager ERROR: Failed to allocate LCD object!");
        return false;
    }

    lcd->init();
    lcd->backlight();
    lcd->clear();

    initialized = true;
    Serial.println("DisplayManager: Initialization successful.");
    return true;
}

bool DisplayManager::isInitialized() const {
    return initialized;
}

// --- Métodos de Controle --- (Sem alterações aqui)
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
        if (enable) lcd->backlight(); else lcd->noBacklight();
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager ERROR: Mutex timeout on setBacklight().");
    }
}

// --- Métodos de Impressão --- (Sem alterações na lógica principal)
void DisplayManager::printLine(uint8_t line, const char* format, ...) {
     if (!initialized || !lcd || line >= lcdRows) return;
     char buffer[lcdCols + 1];
     va_list args;
     va_start(args, format);
     vsnprintf(buffer, sizeof(buffer), format, args);
     va_end(args);

    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        lcd->setCursor(0, line);
        lcd->print(buffer);
        int charsToPrint = strlen(buffer);
        for (int i = charsToPrint; i < lcdCols; ++i) {
            lcd->print(" ");
        }
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager ERROR: Mutex timeout on printLine().");
    }
}

void DisplayManager::showError(const char* message) {
    uint8_t errorLine = (lcdRows > 0) ? lcdRows - 1 : 0;
    printLine(errorLine, "Error: %s", message ? message : "Unknown");
}

// --- Métodos de Status Específico --- (showNtpSynced alterado)

void DisplayManager::showBooting() {
    printLine(0, "Booting...");
    if (lcdRows > 1) printLine(1, "");
}

void DisplayManager::showConnectingWiFi() {
    printLine(0, "Connecting WiFi");
    if (lcdRows > 1) printLine(1, ".");
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
    struct tm now = {0};
    if (lcdRows > 1) {
        // <<< USA o membro timeService injetado
        if (timeService.getCurrentTime(now)) {
            printLine(1, "%02d:%02d", now.tm_hour, now.tm_min);
        } else {
            printLine(1, "--:--"); // Hora não disponível
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

// --- Atualização Principal --- (showSensorData alterado)

void DisplayManager::showSensorData(float temp, float airHum, float soilHum) {
    if (!initialized || !lcd) return;

    struct tm now = {0};
    // <<< USA o membro timeService injetado
    bool timeAvailable = timeService.getCurrentTime(now);

    // Verificar mudanças
    bool tempChanged = isnan(temp) != isnan(this->lastTemp) || (!isnan(temp) && abs(temp - this->lastTemp) > 0.1f);
    bool airHumChanged = isnan(airHum) != isnan(this->lastAirHum) || (!isnan(airHum) && abs(airHum - this->lastAirHum) > 0.5f);
    bool soilHumChanged = isnan(soilHum) != isnan(this->lastSoilHum) || (!isnan(soilHum) && abs(soilHum - this->lastSoilHum) > 0.5f);
    bool humChanged = airHumChanged || soilHumChanged;
    // Atualiza se minuto mudou OU se disponibilidade da hora mudou (de --:-- para HH:MM ou vice-versa)
    bool minuteChanged = (timeAvailable && now.tm_min != this->lastMinute) || (timeAvailable != (this->lastMinute >= 0));


    if (!tempChanged && !humChanged && !minuteChanged) {
        return;
    }

    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        // Atualiza Linha 0 (Temperatura e Hora)
        if (tempChanged || minuteChanged) {
            char line0Str[lcdCols + 1] = {0};
            char tempPart[12] = {0};
            char timePart[10] = {0};

            if (!isnan(temp)) { snprintf(tempPart, sizeof(tempPart), "T:%.1fC", temp); this->lastTemp = temp; }
            else { snprintf(tempPart, sizeof(tempPart), "T:ERR"); this->lastTemp = NAN; }

            if (timeAvailable) { snprintf(timePart, sizeof(timePart), " H:%02d:%02d", now.tm_hour, now.tm_min); this->lastMinute = now.tm_min; }
            else { snprintf(timePart, sizeof(timePart), " H:--:--"); this->lastMinute = -1; } // Usar -1 para indicar hora não disponível

            snprintf(line0Str, sizeof(line0Str), "%s%s", tempPart, timePart);
            lcd->setCursor(0, 0); lcd->print(line0Str);
            for (int i = strlen(line0Str); i < lcdCols; ++i) lcd->print(" ");
        }

        // Atualiza Linha 1 (Umidades)
        if (humChanged && lcdRows > 1) {
            char line1Str[lcdCols + 1] = {0};
            char airPart[12] = {0};
            char soilPart[12] = {0};

            if (!isnan(airHum)) { snprintf(airPart, sizeof(airPart), "Air:%.0f%% ", airHum); this->lastAirHum = airHum; }
            else { snprintf(airPart, sizeof(airPart), "Air:ERR "); this->lastAirHum = NAN; }

            if (!isnan(soilHum)) { snprintf(soilPart, sizeof(soilPart), "Sol:%.0f%%", soilHum); this->lastSoilHum = soilHum; }
            else { snprintf(soilPart, sizeof(soilPart), "Sol:ERR"); this->lastSoilHum = NAN; }

            snprintf(line1Str, sizeof(line1Str), "%s%s", airPart, soilPart);
            lcd->setCursor(0, 1); lcd->print(line1Str);
            for (int i = strlen(line1Str); i < lcdCols; ++i) lcd->print(" ");
        }
        xSemaphoreGive(lcdMutex.get());
    } else {
        Serial.println("DisplayManager WARN: Mutex timeout on showSensorData().");
    }
}

// --- updateSpinner --- (Sem alterações)
void DisplayManager::updateSpinner() {
    if (!initialized || !lcd) return;
    this->spinnerCounter = (this->spinnerCounter + 1) % strlen(SPINNER_GLYPHS);
    if (xSemaphoreTake(lcdMutex.get(), MUTEX_TIMEOUT) == pdTRUE) {
        uint8_t spinnerCol = (lcdCols > 0) ? lcdCols - 1 : 0;
        uint8_t spinnerRow = (lcdRows > 0) ? lcdRows - 1 : 0;
        lcd->setCursor(spinnerCol, spinnerRow);
        lcd->print(SPINNER_GLYPHS[this->spinnerCounter]);
        xSemaphoreGive(lcdMutex.get());
    } else {
       // Serial.println("DisplayManager WARN: Mutex timeout on updateSpinner().");
    }
}

} // namespace GrowController