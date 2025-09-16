#include "config.h"
#include "logger.h"
#include "imu_manager.h"
#include "ble_service.h"

#if !TEST_MODE_ENABLED
#include <CodeCell.h>
CodeCell myCodeCell; // Global instance for IMU manager to use
#endif

// Phase 1 Testing: IMU Manager + BLE Service (placeholder)
// This validates IMU initialization and data reading using vendor patterns

ImuManager imu;
BleService ble;

void setup() {
    // Initialize logger first
    Logger::init();

    // Log boot information
    Logger::logBoot();

    LOG_INFO("=== Phase 1 Testing: IMU + BLE Placeholder ===");
    LOG_INFO("Device: %s", DEVICE_NAME);
    LOG_INFO("Motion threshold: %.2f g", MOTION_THRESHOLD);

    // Setup built-in LED for blinking (visual confirmation)
    pinMode(LED_BUILTIN, OUTPUT);

    // Initialize IMU with vendor patterns
    LOG_INFO("Initializing IMU manager...");
    if (!imu.init()) {
        LOG_ERROR("IMU initialization failed!");
        while (1) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }

    // Initialize BLE placeholder
    LOG_INFO("Initializing BLE service...");
    if (!ble.init()) {
        LOG_ERROR("BLE initialization failed!");
    }

    // Wait for fusion to be ready
    LOG_INFO("Waiting for fusion engine...");
    if (imu.waitForFusionReady()) {
        LOG_INFO("Fusion engine ready!");
    } else {
        LOG_WARN("Fusion engine not ready, continuing anyway...");
    }

    LOG_INFO("Phase 1 test setup complete - entering main loop");
}

void loop() {
    static unsigned long lastLog = 0;
    static unsigned long lastBlink = 0;
    static bool ledState = false;

    unsigned long now = millis();

    // Blink LED every 500ms to show we're alive
    if (now - lastBlink >= 500) {
        lastBlink = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }

    // Update BLE service
    ble.update();

    // Update IMU at configured rate
    if (imu.update(ACTIVE_IMU_RATE)) {
        const ImuManager::SensorData& data = imu.getData();

        // Check for motion
        bool motion = imu.checkMotion();

        // Send data to BLE (placeholder for now)
        ble.sendImuData(data);

        // Log sensor data every 2 seconds (not every reading)
        if (now - lastLog >= 2000) {
            lastLog = now;

            LOG_INFO("IMU: quat=[%.3f,%.3f,%.3f] accel=[%.3f,%.3f,%.3f] motion=%.3f fusion=%s",
                     data.qx, data.qy, data.qz,
                     data.ax, data.ay, data.az,
                     imu.getLastMotionDelta(),
                     data.fusionReady ? "READY" : "NOT_READY");

            if (motion) {
                LOG_INFO("MOTION DETECTED!");
            }

            Logger::logMemory();
        }
    }

    delay(10); // Small delay for cooperative multitasking
}