// src/utils/logger.cpp
#include "logger.hpp"
#include <stdio.h> // Para vsnprintf
// #include "freertos/FreeRTOS.h" // Já incluído via logger.hpp
// #include "freertos/semphr.h"   // Já incluído via logger.hpp
#include <time.h> // Para timestamp (se habilitado)
#include <sys/time.h> // Para gettimeofday (se habilitado)

namespace GrowController {

Print* Logger::output = &Serial; // Default output
LogLevel Logger::currentLevel = LogLevel::INFO; // Default log level
SemaphoreHandle_t Logger::logMutex = nullptr;
bool Logger::mutexInitialized = false;

// Logger::init agora aceita Print&
void Logger::init(Print& printPort, LogLevel level) {
    output = &printPort;
    currentLevel = level;
    if (!mutexInitialized) {
        logMutex = xSemaphoreCreateMutex();
        if (logMutex == nullptr) {
            if (output) output->println("Logger ERROR: Failed to create mutex!");
        } else {
            mutexInitialized = true;
        }
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::log(LogLevel level, const char* levelStr, const char* format, va_list args) {
    if (!output || level < currentLevel || currentLevel == LogLevel::NONE) {
        return;
    }

    bool mutexTaken = false;
    if (mutexInitialized && logMutex != nullptr) {
        if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            mutexTaken = true;
        } else {
            return; // Não conseguiu pegar o mutex, descarta a mensagem
        }
    }

    char buffer[256];

    // #define LOGGER_INCLUDE_TIMESTAMP // Descomente para habilitar timestamps
    #ifdef LOGGER_INCLUDE_TIMESTAMP
        char timePrefix[32];
        struct timeval tv;
        // gettimeofday só é útil se o tempo estiver sincronizado
        if (time(NULL) > 1600000000) { // Checagem básica se o tempo parece válido
            gettimeofday(&tv, NULL);
            strftime(timePrefix, sizeof(timePrefix), "%H:%M:%S", localtime(&tv.tv_sec));
            snprintf(buffer, sizeof(buffer), "[%s.%03ld] ", timePrefix, tv.tv_usec / 1000);
            output->print(buffer);
        }
    #endif

    output->print("[");
    output->print(levelStr);
    output->print("] ");

    vsnprintf(buffer, sizeof(buffer), format, args);
    output->println(buffer);
    
    // RTTI está desabilitado, então dynamic_cast não funciona.
    // Print::flush() existe e fará o que for apropriado para a classe concreta.
    if (level == LogLevel::ERROR) {
        output->flush();
    }

    if (mutexTaken) {
        xSemaphoreGive(logMutex);
    }
}

void Logger::debug(const char* format, ...) {
    if (currentLevel > LogLevel::DEBUG && currentLevel != LogLevel::NONE) return; // Correção: só pula se currentLevel for maior E não for NONE
    if (currentLevel == LogLevel::NONE) return; // Se NONE, não loga nada
    va_list args;
    va_start(args, format);
    log(LogLevel::DEBUG, "DEBUG", format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    if (currentLevel > LogLevel::INFO && currentLevel != LogLevel::NONE) return;
    if (currentLevel == LogLevel::NONE) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::INFO, "INFO", format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) {
    if (currentLevel > LogLevel::WARN && currentLevel != LogLevel::NONE) return;
    if (currentLevel == LogLevel::NONE) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::WARN, "WARN", format, args);
    va_end(args);
}

void Logger::error(const char* format, ...) {
    // Erros são logados a menos que o nível seja NONE.
    // A verificação `level < currentLevel` em `log()` já cuida disso.
    // O `if` adicional aqui é redundante.
    va_list args;
    va_start(args, format);
    log(LogLevel::ERROR, "ERROR", format, args);
    va_end(args);
}

} // namespace GrowController