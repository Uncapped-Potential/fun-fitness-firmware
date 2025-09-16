// Test compilation in isolation - simulates Arduino IDE compilation check
#define TESTING 1  // Enable test mode

#include "config.h"
#include "logger.h"
#include "imu_manager.h"
#include "ble_service.h"

ImuManager imu;
BleService ble;

void setup() {
    Logger::init();
    Logger::logBoot();

    LOG_INFO("=== Compilation Test ===");

    if (imu.init()) {
        LOG_INFO("IMU manager compilation: PASS");
    }

    if (ble.init()) {
        LOG_INFO("BLE service compilation: PASS");
    }

    LOG_INFO("All modules compiled successfully!");
}

void loop() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (now - lastUpdate >= 1000) {
        lastUpdate = now;

        // Test IMU update
        if (imu.update(10)) {
            const ImuManager::SensorData& data = imu.getData();
            LOG_INFO("Test IMU data: quat=[%.3f,%.3f,%.3f] valid=%s",
                     data.qx, data.qy, data.qz, data.valid ? "YES" : "NO");

            // Test BLE send
            ble.sendImuData(data);
        }

        Logger::logMemory();
    }

    delay(100);
}