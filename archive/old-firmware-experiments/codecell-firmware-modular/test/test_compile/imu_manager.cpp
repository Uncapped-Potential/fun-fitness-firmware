#include "imu_manager.h"
#include <cmath>

ImuManager::ImuManager()
    : initialized(false)
    , fusionReady(false)
    , removeGravity(false)
    , motionBaselineSet(false)
    , lastAx(0), lastAy(0), lastAz(0)
    , lastMotionDelta(0)
    , lastMotionTime(0)
    , consecutiveMotionCount(0)
    , lastUpdate(0)
#if !TEST_MODE_ENABLED
    , codecell(nullptr)
#endif
{
    currentData = {0};
}

bool ImuManager::init() {
    if (initialized) {
        LOG_WARN("IMU already initialized");
        return true;
    }

    LOG_INFO("Initializing IMU with vendor patterns...");

#if !TEST_MODE_ENABLED
    // Use external CodeCell instance - don't create our own
    extern CodeCell myCodeCell;
    codecell = &myCodeCell;

    // Wait for sensor stabilization as recommended by vendor
    LOG_INFO("IMU stabilization delay (%d ms)...", IMU_STABILIZATION_MS);
    delay(IMU_STABILIZATION_MS);

    // Single Init() call as per vendor guidance
    // Use full sensor suite including linear acceleration for gravity removal
    LOG_INFO("Calling codecell.Init() with full sensor suite...");
    codecell->Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION +
                   MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);

    // Give sensors time to initialize
    delay(1000);

    // Test if initialization was successful
    if (!testSensorValues()) {
        LOG_WARN("Full sensor suite failed, trying accelerometer only...");
        delay(1000);
        codecell->Init(MOTION_ACCELEROMETER);
        delay(1000);

        if (!testSensorValues()) {
            LOG_ERROR("IMU initialization failed completely");
            return false;
        }
        LOG_INFO("Accelerometer-only initialization successful");
    } else {
        LOG_INFO("Full sensor suite initialization successful");
    }
#else
    // Test mode - just mark as initialized
    LOG_INFO("Test mode: IMU initialization simulated");
#endif

    initialized = true;
    lastMotionTime = millis();

    LOG_INFO("IMU initialization complete");
    return true;
}

bool ImuManager::update(int rateHz) {
    if (!initialized) {
        LOG_ERROR("IMU not initialized");
        return false;
    }

    // Rate limiting
    unsigned long now = millis();
    unsigned long interval = 1000 / rateHz;
    if (now - lastUpdate < interval) {
        return false; // No update needed yet
    }
    lastUpdate = now;

#if !TEST_MODE_ENABLED
    // Single Run() call per update as per vendor guidance
    if (!codecell->Run(rateHz)) {
        return false; // No new data available
    }

    // Read sensor data
    float qr, qi, qj, qk;
    codecell->Motion_RotationVectorRead(qr, qi, qj, qk);

    // Store quaternion XYZ components (W calculated by receiver)
    currentData.qx = qi;
    currentData.qy = qj;
    currentData.qz = qk;

    // Read accelerometer data based on gravity removal setting
    if (removeGravity) {
        codecell->Motion_LinearAccRead(currentData.ax, currentData.ay, currentData.az);
    } else {
        codecell->Motion_AccelerometerRead(currentData.ax, currentData.ay, currentData.az);
    }

    // Read gyroscope data
    codecell->Motion_GyroRead(currentData.gx, currentData.gy, currentData.gz);

    // Read battery level
    currentData.battery = codecell->BatteryLevelRead();

    // Check if fusion engine is producing valid quaternions
    float quat_magnitude = sqrt(currentData.qx * currentData.qx +
                               currentData.qy * currentData.qy +
                               currentData.qz * currentData.qz);

    // Fusion is ready if quaternion components have reasonable magnitude
    // (W component will be calculated as sqrt(1 - x² - y² - z²))
    bool quatValid = (quat_magnitude > 0.01 && quat_magnitude < 1.0);

    if (quatValid && !fusionReady) {
        fusionReady = true;
        LOG_INFO("IMU fusion engine ready - quaternion magnitude: %.3f", quat_magnitude);
    }

    currentData.fusionReady = fusionReady;
    currentData.valid = (currentData.ax != 0 || currentData.ay != 0 || currentData.az != 0);

#else
    // Test mode - generate mock data
    currentData.qx = 0.1f * sin(now * 0.001f);
    currentData.qy = 0.1f * cos(now * 0.001f);
    currentData.qz = 0.05f;
    currentData.ax = 0.0f;
    currentData.ay = 0.0f;
    currentData.az = 1.0f; // 1g downward
    currentData.gx = 0.0f;
    currentData.gy = 0.0f;
    currentData.gz = 0.0f;
    currentData.battery = 85;
    currentData.valid = true;
    fusionReady = true;
    currentData.fusionReady = true;
#endif

    return true;
}

bool ImuManager::waitForFusionReady(unsigned long timeoutMs) {
    if (fusionReady) return true;

    LOG_INFO("Waiting for fusion ready (timeout: %lu ms)...", timeoutMs);
    unsigned long startTime = millis();

    while (millis() - startTime < timeoutMs) {
        if (update(10)) { // Update at 10Hz while waiting
            if (fusionReady) {
                LOG_INFO("Fusion ready after %lu ms", millis() - startTime);
                return true;
            }
        }
        delay(100);
    }

    LOG_WARN("Fusion ready timeout after %lu ms", timeoutMs);
    return false;
}

bool ImuManager::checkMotion(float threshold) {
    if (!currentData.valid) return false;

    if (!motionBaselineSet) {
        updateMotionBaseline();
        return false; // No motion on first reading
    }

    // Calculate change in acceleration from previous reading
    float deltaAx = fabsf(currentData.ax - lastAx);
    float deltaAy = fabsf(currentData.ay - lastAy);
    float deltaAz = fabsf(currentData.az - lastAz);
    lastMotionDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);

    updateMotionBaseline();

    if (lastMotionDelta > threshold) {
        consecutiveMotionCount++;
        if (consecutiveMotionCount >= MOTION_REQUIRED_COUNT) {
            lastMotionTime = millis();
            LOG_DEBUG("Motion detected: delta=%.3f (threshold=%.3f)", lastMotionDelta, threshold);
            return true;
        }
    } else {
        consecutiveMotionCount = 0;
    }

    return false;
}

bool ImuManager::testSensorValues() {
#if !TEST_MODE_ENABLED
    float testAx, testAy, testAz;
    codecell->Motion_AccelerometerRead(testAx, testAy, testAz);

    bool accelWorking = (testAx != 0.0 || testAy != 0.0 || testAz != 0.0);
    LOG_DEBUG("Sensor test: accel=[%.3f, %.3f, %.3f] working=%d",
              testAx, testAy, testAz, accelWorking);

    return accelWorking;
#else
    return true; // Always pass in test mode
#endif
}

void ImuManager::updateMotionBaseline() {
    lastAx = currentData.ax;
    lastAy = currentData.ay;
    lastAz = currentData.az;

    if (!motionBaselineSet) {
        motionBaselineSet = true;
        LOG_DEBUG("Motion baseline set: [%.3f, %.3f, %.3f]", lastAx, lastAy, lastAz);
    }
}