// logger.hpp
#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <Arduino.h> // Para Print
#include <stdarg.h>

namespace GrowController {
enum class LogLevel { DEBUG, INFO, WARN, ERROR, NONE };

class Logger {
public:
    static void init(HardwareSerial& serialPort = Serial, LogLevel level = LogLevel::INFO);
    static void setLevel(LogLevel level);

    static void debug(const char* format, ...);
    static void info(const char* format, ...);
    static void warn(const char* format, ...);
    static void error(const char* format, ...);

private:
    static Print* output;
    static LogLevel currentLevel;
    static SemaphoreHandle_t logMutex; // Para proteger Serial de múltiplas tarefas

    static void log(LogLevel level, const char* levelStr, const char* format, va_list args);
};
} // namespace GrowController
#endif // LOGGER_HPP

// logger.cpp - Implementar métodos, usar mutex antes de printar