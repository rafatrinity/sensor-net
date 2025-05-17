// src/utils/logger.hpp
#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <Arduino.h> // Para Print, HardwareSerial (se necessário para outros contextos), Serial
#include <stdarg.h>  // Para va_list, etc.
#include "freertos/FreeRTOS.h" // Para SemaphoreHandle_t
#include "freertos/semphr.h"   // Para xSemaphoreCreateMutex, etc.


namespace GrowController {
enum class LogLevel { DEBUG, INFO, WARN, ERROR, NONE };

class Logger {
public:
    // Alterado HardwareSerial& para Print& para compatibilidade com HWCDC (Serial no C3)
    static void init(Print &printPort = Serial, LogLevel level = LogLevel::INFO);
    static void setLevel(LogLevel level);

    static void debug(const char* format, ...);
    static void info(const char* format, ...);
    static void warn(const char* format, ...);
    static void error(const char* format, ...);

private:
    static Print* output;
    static LogLevel currentLevel;
    static SemaphoreHandle_t logMutex;
    static bool mutexInitialized; // Flag para garantir que o mutex seja criado apenas uma vez

    static void log(LogLevel level, const char* levelStr, const char* format, va_list args);
};
} // namespace GrowController
#endif // LOGGER_HPP