#include "imu_manager.h"
#include <Arduino.h>

// Static counter initialization
unsigned long ImuManager::accelUpdateCount = 0;
unsigned long ImuManager::gyroUpdateCount = 0;
unsigned long ImuManager::quatUpdateCount = 0;

ImuManager::ImuManager()
    : codeCell(nullptr)
    , initialized(false)
    , newDataAvailable(false)
    , lastAx(0), lastAy(0), lastAz(0)
    , baselineSet(false)
    , lastMotionTime(0)
{
    currentData = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0.0, false, 0};
}

bool ImuManager::init() {
    Serial.println("Initializing IMU manager...");

    // Get reference to global CodeCell instance
    extern CodeCell myCodeCell;
    codeCell = &myCodeCell;

    // Verify initialization worked
    float testAx, testAy, testAz;
    codeCell->Motion_AccelerometerRead(testAx, testAy, testAz);

    if (testAx == 0.0 && testAy == 0.0 && testAz == 0.0) {
        Serial.println("ERROR: IMU manager - accelerometer initialization failed!");
        return false;
    }

    Serial.printf("IMU manager - initial accel reading: [%.3f, %.3f, %.3f]\n", testAx, testAy, testAz);
    Serial.println("IMU manager initialization complete");

    initialized = true;
    lastMotionTime = millis();
    return true;
}

bool ImuManager::update() {
    if (!initialized) return false;

    // Call all inline performance-critical functions
    readSensors();
    detectMotion();
    validateQuaternion();

    newDataAvailable = true;
    return true;
}

void ImuManager::updateCounters(unsigned long& accelCount, unsigned long& gyroCount, unsigned long& quatCount) {
    accelCount = accelUpdateCount;
    gyroCount = gyroUpdateCount;
    quatCount = quatUpdateCount;

    // Reset counters for next measurement
    accelUpdateCount = 0;
    gyroUpdateCount = 0;
    quatUpdateCount = 0;
}

bool ImuManager::isMotionActive() const {
    return (millis() - lastMotionTime) < SLEEP_TIMEOUT;
}