#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#include "config.h"
#include "logger.h"

#if !TEST_MODE_ENABLED
#include <CodeCell.h>
#endif

class ImuManager {
public:
    struct SensorData {
        // Quaternion (X,Y,Z components - W calculated by receiver)
        float qx, qy, qz;

        // Accelerometer (raw or linear based on config)
        float ax, ay, az;

        // Gyroscope
        float gx, gy, gz;

        // Battery level
        int battery;

        // Validity flags
        bool valid;
        bool fusionReady;
    };

    ImuManager();

    // Initialize sensors - call ONCE in setup()
    bool init();

    // Update sensor data - call ONCE per loop with desired rate
    bool update(int rateHz);

    // Get latest sensor data
    const SensorData& getData() const { return currentData; }

    // Check if fusion engine is ready for quaternion output
    bool isFusionReady() const { return fusionReady; }

    // Wait for fusion engine to be ready (blocking)
    bool waitForFusionReady(unsigned long timeoutMs = FUSION_READY_TIMEOUT);

    // Set gravity removal mode
    void setGravityRemoval(bool enabled) { removeGravity = enabled; }
    bool getGravityRemoval() const { return removeGravity; }

    // Motion detection
    bool checkMotion(float threshold = MOTION_THRESHOLD);
    float getLastMotionDelta() const { return lastMotionDelta; }

private:
    SensorData currentData;
    bool initialized;
    bool fusionReady;
    bool removeGravity;

    // Motion detection state
    bool motionBaselineSet;
    float lastAx, lastAy, lastAz;
    float lastMotionDelta;
    unsigned long lastMotionTime;
    int consecutiveMotionCount;

    // Rate limiting
    unsigned long lastUpdate;

#if !TEST_MODE_ENABLED
    CodeCell* codecell;
#endif

    bool testSensorValues();
    void updateMotionBaseline();
};

#endif // IMU_MANAGER_H