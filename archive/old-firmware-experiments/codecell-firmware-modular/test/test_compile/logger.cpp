#include "logger.h"
#include "esp_ota_ops.h"
#include <stdio.h>
#include <stdarg.h>

bool Logger::initialized = false;

void Logger::init() {
    if (!initialized) {
        Serial.begin(115200);
        delay(1000);
        initialized = true;
        Serial.println();
        Serial.println("=================================");
        Serial.println("CodeCell Modular Firmware v1.0");
        Serial.println("=================================");
    }
}

void Logger::logBoot() {
    if (!initialized) return;

    esp_reset_reason_t reset_reason = esp_reset_reason();
    const esp_partition_t* active_partition = esp_ota_get_running_partition();
    size_t free_heap = esp_get_free_heap_size();

    Serial.printf("BOOT: Reset reason: %s\n", getResetReasonString(reset_reason));
    if (active_partition) {
        Serial.printf("BOOT: Active partition: %s (size: %d bytes)\n",
                     active_partition->label, active_partition->size);
    }
    Serial.printf("BOOT: Free heap: %d bytes\n", free_heap);
}

void Logger::logConnect(uint16_t mtu, uint16_t interval, uint16_t latency, uint16_t timeout) {
    if (!initialized) return;
    Serial.printf("BLE_CONNECT: MTU=%d, interval=%dms, latency=%d, timeout=%dms\n",
                 mtu, interval, latency, timeout);
}

void Logger::logOtaEvent(const char* event, uint32_t param1, uint32_t param2) {
    if (!initialized) return;
    Serial.printf("OTA_%s: %u %u\n", event, param1, param2);
}

void Logger::logPowerTransition(const char* from, const char* to) {
    if (!initialized) return;
    Serial.printf("POWER: %s -> %s\n", from, to);
}

void Logger::logMemory() {
    if (!initialized) return;
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    Serial.printf("MEMORY: free=%d, min_free=%d\n", free_heap, min_free);
}

void Logger::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_ERROR, "ERROR", format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_WARN, "WARN", format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_INFO, "INFO", format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_DEBUG, "DEBUG", format, args);
    va_end(args);
}

void Logger::log(int level, const char* prefix, const char* format, va_list args) {
    if (!initialized || level > LOG_LEVEL) return;

    Serial.printf("[%s] ", prefix);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.println(buffer);
}

const char* Logger::getResetReasonString(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "Power-on";
        case ESP_RST_EXT: return "External reset";
        case ESP_RST_SW: return "Software reset";
        case ESP_RST_PANIC: return "Exception/panic";
        case ESP_RST_INT_WDT: return "Interrupt watchdog";
        case ESP_RST_TASK_WDT: return "Task watchdog";
        case ESP_RST_WDT: return "Other watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO reset";
        default: return "Unknown";
    }
}