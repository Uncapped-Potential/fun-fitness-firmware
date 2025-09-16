#ifndef LOGGER_H
#define LOGGER_H

#include "config.h"
#include "esp_system.h"
#include <HardwareSerial.h>

class Logger {
public:
    static void init();
    static void logBoot();
    static void logConnect(uint16_t mtu, uint16_t interval, uint16_t latency, uint16_t timeout);
    static void logOtaEvent(const char* event, uint32_t param1 = 0, uint32_t param2 = 0);
    static void logPowerTransition(const char* from, const char* to);
    static void logMemory();

    // General logging with levels
    static void error(const char* format, ...);
    static void warn(const char* format, ...);
    static void info(const char* format, ...);
    static void debug(const char* format, ...);

private:
    static void log(int level, const char* prefix, const char* format, va_list args);
    static const char* getResetReasonString(esp_reset_reason_t reason);
    static bool initialized;
};

// Convenience macros
#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(...) Logger::error(__VA_ARGS__)
#else
#define LOG_ERROR(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(...) Logger::warn(__VA_ARGS__)
#else
#define LOG_WARN(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(...) Logger::info(__VA_ARGS__)
#else
#define LOG_INFO(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) Logger::debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#endif // LOGGER_H