#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#include "config.h"
#include <CodeCell.h>
#include <Arduino.h>

class ImuManager {
public:
    struct SensorData {
        float qr, qx, qy, qz;     // Quaternion
        float ax, ay, az;         // Accelerometer
        float gx, gy, gz;         // Gyroscope
        bool motionDetected;
        float motionDelta;
        bool validQuaternion;
        unsigned long timestamp;
    };

    ImuManager();
    bool init();
    bool update();

    inline const SensorData& getData() const { return currentData; }
    inline bool hasNewData() const { return newDataAvailable; }
    inline void clearNewDataFlag() { newDataAvailable = false; }

    void updateCounters(unsigned long& accelCount, unsigned long& gyroCount, unsigned long& quatCount);
    bool isMotionActive() const;

private:
    CodeCell* codeCell;
    SensorData currentData;
    bool initialized;
    bool newDataAvailable;

    // Motion detection state
    float lastAx, lastAy, lastAz;
    bool baselineSet;
    unsigned long lastMotionTime;

    // Performance counters (inline for zero overhead)
    static unsigned long accelUpdateCount;
    static unsigned long gyroUpdateCount;
    static unsigned long quatUpdateCount;

    // Inline performance-critical functions
    inline bool readSensors();
    inline bool detectMotion();
    inline bool validateQuaternion();
};

// Performance-critical sensor reading - fully inlined
inline bool ImuManager::readSensors() {
    // Read quaternion data (main goal)
    codeCell->Motion_RotationVectorRead(currentData.qr, currentData.qx, currentData.qy, currentData.qz);

    // Read accelerometer for motion detection
    codeCell->Motion_AccelerometerRead(currentData.ax, currentData.ay, currentData.az);
    accelUpdateCount++;

    // Read gyroscope for complete IMU data
    codeCell->Motion_GyroRead(currentData.gx, currentData.gy, currentData.gz);
    gyroUpdateCount++;

    currentData.timestamp = millis();
    return true;
}

// Performance-critical motion detection - fully inlined
inline bool ImuManager::detectMotion() {
    bool currentMotion = false;
    currentData.motionDelta = 0.0;

    if (baselineSet) {
        float deltaAx = fabs(currentData.ax - lastAx);
        float deltaAy = fabs(currentData.ay - lastAy);
        float deltaAz = fabs(currentData.az - lastAz);
        float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);

        currentData.motionDelta = totalDelta;

        if (totalDelta > MOTION_THRESHOLD) {
            currentMotion = true;
            lastMotionTime = millis();
        }
    }

    // Update baseline for next comparison
    lastAx = currentData.ax;
    lastAy = currentData.ay;
    lastAz = currentData.az;
    baselineSet = true;

    currentData.motionDetected = currentMotion;
    return currentMotion;
}

// Performance-critical quaternion validation - fully inlined
inline bool ImuManager::validateQuaternion() {
    static float lastQr = 0, lastQx = 0, lastQy = 0, lastQz = 0;

    // Track quaternion changes for rate monitoring
    if (currentData.qr != lastQr || currentData.qx != lastQx ||
        currentData.qy != lastQy || currentData.qz != lastQz) {
        quatUpdateCount++;
        lastQr = currentData.qr; lastQx = currentData.qx;
        lastQy = currentData.qy; lastQz = currentData.qz;
    }

    // Validate quaternion magnitude
    float quatMagnitude = sqrt(currentData.qr*currentData.qr + currentData.qx*currentData.qx +
                              currentData.qy*currentData.qy + currentData.qz*currentData.qz);
    currentData.validQuaternion = (quatMagnitude > 0.1 && quatMagnitude < 1.1);

    return currentData.validQuaternion;
}

#endif // IMU_MANAGER_H