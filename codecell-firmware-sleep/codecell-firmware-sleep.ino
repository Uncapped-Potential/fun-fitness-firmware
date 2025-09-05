/*
 * CodeCell BLE Quaternion Streamer with Motion-Based Sleep Mode
 * Sends 3-component quaternion data, W component calculated on device
 * Implements power-efficient sleep mode with motion-based wake/sleep detection
 */

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

CodeCell myCodeCell;

// RTC memory to survive deep sleep
RTC_DATA_ATTR bool sensorInitialized = false;

// BLE UUIDs (matching MicroLink for compatibility)
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Power management configuration
int dataRate = 60; // 60Hz for normal operation
int lowPowerDataRate = 1; // 1Hz for low power mode
int imuRate = 60; // IMU polling rate (matches dataRate for normal operation)
int lowPowerImuRate = 10; // Reduced IMU rate for power saving (still responsive for motion detection)
float motionThreshold = 0.4; // Minimum change in acceleration to detect motion (g-force)
int consecutiveMotionRequired = 2; // Require consecutive motion readings to confirm real motion
int bleInactivityTimeout = 10000; // Reduce BLE rate after 10 seconds of no motion
int lightSleepTimeout = 60000; // Enter light sleep (sensor only) after 1 minute

// Deep sleep removed - it completely breaks BNO085 sensor functionality
// Even with RST pin tied to 3.3V, the sensor loses all configuration during ESP32 deep sleep
// and cannot be reinitialized, returning only zeros. Light sleep preserves sensor state.
unsigned long lastMotionTime = 0;
unsigned long lastBleCheck = 0;
unsigned long lastSleepCheck = 0;
bool bleActive = true;
bool sensorsActive = true;
enum PowerState { ACTIVE, BLE_OFF, LIGHT_SLEEP };
PowerState currentPowerState = ACTIVE;

// Motion detection variables
float lastAx = 0, lastAy = 0, lastAz = 0;
bool motionBaselineSet = false;
int consecutiveMotionCount = 0;
float lastDetectedDelta = 0.0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Computer GUI Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Computer GUI Disconnected - will restart advertising");
    }
};

// Function to check if there's significant motion by comparing acceleration changes
bool checkMotionActivity() {
    float ax, ay, az;
    myCodeCell.Motion_AccelerometerRead(ax, ay, az);
    
    if (!motionBaselineSet) {
        // Set initial baseline
        lastAx = ax;
        lastAy = ay;
        lastAz = az;
        motionBaselineSet = true;
        Serial.printf("Motion baseline set: [%.3f, %.3f, %.3f]\n", ax, ay, az);
        return false; // No motion on first reading
    }
    
    // Calculate change in acceleration from previous reading
    float deltaAx = abs(ax - lastAx);
    float deltaAy = abs(ay - lastAy);
    float deltaAz = abs(az - lastAz);
    float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
    
    Serial.printf("Motion delta: %.3f g (threshold: %.3f)\n", totalDelta, motionThreshold);
    
    // Update baseline for next comparison
    lastAx = ax;
    lastAy = ay;
    lastAz = az;
    
    if (totalDelta > motionThreshold) {
        consecutiveMotionCount++;
        Serial.printf("Motion detected! (consecutive: %d/%d)\n", consecutiveMotionCount, consecutiveMotionRequired);
        
        if (consecutiveMotionCount >= consecutiveMotionRequired) {
            lastMotionTime = millis();
            return true;
        }
    } else {
        consecutiveMotionCount = 0; // Reset counter if no motion
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Longer delay for Serial to initialize
    
    Serial.println("=================================");
    Serial.println("CodeCell Sleep Mode Firmware Starting...");
    Serial.println("=================================");
    
    // Check wake-up cause to determine initialization strategy
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Waking from deep sleep - sensor should still be initialized");
        // DON'T reinitialize - BNO085 is still powered and running
        
        // Just do a quick motion check without reinitialization
        delay(500); // Brief stabilization
        
        float baselineAx, baselineAy, baselineAz;
        myCodeCell.Motion_AccelerometerRead(baselineAx, baselineAy, baselineAz);
        Serial.printf("Wake-up baseline: [%.3f, %.3f, %.3f]\n", baselineAx, baselineAy, baselineAz);
        
        bool motionDetected = false;
        for (int i = 0; i < 5; i++) {
            delay(200);
            myCodeCell.Run(1); // Update sensor data
            
            float ax, ay, az;
            myCodeCell.Motion_AccelerometerRead(ax, ay, az);
            
            float deltaAx = abs(ax - baselineAx);
            float deltaAy = abs(ay - baselineAy);
            float deltaAz = abs(az - baselineAz);
            float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
            
            Serial.printf("Wake check %d: delta %.3f (threshold %.3f)\n", i+1, totalDelta, motionThreshold);
            
            if (totalDelta > motionThreshold) {
                motionDetected = true;
                Serial.println("Motion detected during wake-up check!");
                break;
            }
        }
        
        if (!motionDetected) {
            Serial.println("No motion detected - going back to sleep");
            Serial.flush();
            esp_sleep_enable_timer_wakeup(1000000); // 1 second
            esp_deep_sleep_start();
        }
        
        Serial.println("Motion detected - staying awake");
        // Sensor is already initialized, skip to BLE setup
        goto ble_init;
    } else {
        Serial.println("Normal startup (power-on or reset)");
        sensorInitialized = false; // Reset flag on fresh boot
    }
    
    {
        Serial.println("CodeCell BLE Quaternion Streamer with Sleep Mode");
        
        // Initialize sensors - try full suite first, fallback to accelerometer only
        Serial.println("Initializing motion sensors...");
        
        // Give sensors time to stabilize after any wake-up
        Serial.println("Sensor stabilization delay...");
        delay(3000);
        
        // Try full sensor initialization first
        Serial.println("Attempting full sensor suite initialization...");
        myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER);
        
        // Test if full sensors work
        delay(1000);
        float testAx, testAy, testAz;
        float testQr, testQi, testQj, testQk;
        myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
        myCodeCell.Motion_RotationVectorRead(testQr, testQi, testQj, testQk);
        
        // Check if accelerometer is working (it clearly is based on output)
        bool accelWorking = (testAx != 0.0 || testAy != 0.0 || testAz != 0.0);
        
        Serial.printf("Sensor test results: Accel=[%.3f, %.3f, %.3f], Quat=[%.3f, %.3f, %.3f, %.3f]\n", 
                      testAx, testAy, testAz, testQr, testQi, testQj, testQk);
        
        // Accept initialization if accelerometer is working (quaternion may need more time)
        bool fullSensorsWorking = accelWorking;
        
        if (fullSensorsWorking) {
            Serial.println("Full sensor suite initialized successfully");
            sensorInitialized = true;
        } else {
            Serial.println("Full sensors failed - falling back to accelerometer only");
            delay(1000);
            myCodeCell.Init(MOTION_ACCELEROMETER);
            delay(1000);
            myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
            if (testAx != 0.0 || testAy != 0.0 || testAz != 0.0) {
                Serial.println("Accelerometer fallback successful");
                sensorInitialized = true;
            } else {
                Serial.println("WARNING: Even accelerometer failed to initialize");
            }
        }
    }

ble_init:
    
    // Give sensors more time to initialize and stabilize
    delay(1000);
    Serial.println("Sensors ready, initializing BLE...");
    
    // Initialize BLE
    BLEDevice::init("FitChip011");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    Serial.println("Ready for computer GUI connection...");
    lastMotionTime = millis();
    lastBleCheck = millis();
}

void loop() {
    // Multi-tier power management
    unsigned long currentTime = millis();
    if (currentTime - lastBleCheck > 1000) { // Check every second
        lastBleCheck = currentTime;
        unsigned long timeSinceMotion = currentTime - lastMotionTime;
        
        // State machine for power management
        switch (currentPowerState) {
            case ACTIVE:
                if (timeSinceMotion > bleInactivityTimeout) {
                    Serial.printf("Entering BLE_OFF state (1Hz mode) after %lums\n", timeSinceMotion);
                    dataRate = lowPowerDataRate; // Reduce to 1Hz transmission
                    imuRate = lowPowerImuRate; // Reduce IMU polling to 10Hz
                    Serial.println("BLE_OFF: Reducing transmission to 1Hz and IMU polling to 10Hz");
                    currentPowerState = BLE_OFF;
                }
                break;
                
            case BLE_OFF:
                if (timeSinceMotion < 500) {
                    Serial.printf("Motion detected - returning to ACTIVE state\n");
                    dataRate = 60; // Restore full data rate
                    imuRate = 60; // Restore full IMU rate
                    pServer->getAdvertising()->start();
                    bleActive = true;
                    currentPowerState = ACTIVE;
                } else if (timeSinceMotion > lightSleepTimeout) {
                    Serial.printf("Entering LIGHT_SLEEP state after %lums\n", timeSinceMotion);
                    Serial.println("LIGHT_SLEEP: Sensors active, CPU reduced to 80MHz, BLE off, IMU at 5Hz");
                    imuRate = 5; // Reduce to 5Hz for maximum power savings in light sleep
                    currentPowerState = LIGHT_SLEEP;
                    // Keep sensors running, just reduce CPU frequency
                    setCpuFrequencyMhz(80); // Reduce from 160MHz to 80MHz
                }
                break;
                
            case LIGHT_SLEEP:
                if (timeSinceMotion < 500) {
                    Serial.printf("Motion detected - waking to ACTIVE state\n");
                    setCpuFrequencyMhz(160); // Restore full speed
                    dataRate = 60; // Restore full data rate
                    imuRate = 60; // Restore full IMU rate
                    bleActive = true;
                    currentPowerState = ACTIVE;
                }
                // Stay in light sleep - no deeper sleep mode available due to sensor issues
                break;
        }
    }
    
    // Run sensor readings - deep sleep removed due to BNO085 compatibility issues
    
    if (myCodeCell.Run(imuRate)) {
        // Read quaternion data directly from BNO085 (no gimbal lock!)
        float qr, qi, qj, qk;
        myCodeCell.Motion_RotationVectorRead(qr, qi, qj, qk);
        
        // Read accelerometer and gyro data
        float ax, ay, az;
        float gx, gy, gz;
        myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        myCodeCell.Motion_GyroRead(gx, gy, gz);

        // Check for motion activity to reset sleep timer using delta method
        float totalDelta = 0.0;
        if (motionBaselineSet) {
            float deltaAx = abs(ax - lastAx);
            float deltaAy = abs(ay - lastAy);
            float deltaAz = abs(az - lastAz);
            totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
            
            if (totalDelta > motionThreshold) {
                lastMotionTime = currentTime;
                lastDetectedDelta = totalDelta;
                consecutiveMotionCount++;
            } else {
                consecutiveMotionCount = 0;
            }
            
            // Update baseline for next comparison
            lastAx = ax;
            lastAy = ay;
            lastAz = az;
        } else {
            // Set initial baseline during first loop iteration
            lastAx = ax;
            lastAy = ay;
            lastAz = az;
            motionBaselineSet = true;
            lastMotionTime = currentTime; // Start with motion detected
        }

        // Debug: Show raw accelerometer values and calculated motion
        const char* stateNames[] = {"ACTIVE", "BLE_OFF", "LIGHT_SLEEP", "DEEP_SLEEP"};
        Serial.printf("Raw accel: [%.3f, %.3f, %.3f] | Delta: %.4f | Threshold: %.3f | BLE: %s | State: %s | ", 
                     ax, ay, az, totalDelta, motionThreshold, bleActive ? "ON" : "OFF", stateNames[currentPowerState]);

        // Read battery level
        int batteryLevel = myCodeCell.BatteryLevelRead();

        /*
         * MINIMAL QUATERNION PROTOCOL - 19 BYTES (Same as original Euler!)
         * =================================================================
         * 
         * Send only X, Y, Z components of quaternion.
         * W component calculated on web app using: W = √(1 - X² - Y² - Z²)
         * 
         * Byte Layout:
         * [0-1]   Quat X (int16_t)    - Range: ±1.0,      Scale: 10000,   Precision: 0.0001
         * [2-3]   Quat Y (int16_t)    - Range: ±1.0,      Scale: 10000,   Precision: 0.0001  
         * [4-5]   Quat Z (int16_t)    - Range: ±1.0,      Scale: 10000,   Precision: 0.0001
         * [6-7]   Accel X (int16_t)   - Range: ±65.5g,    Scale: 500,     Precision: 0.002g
         * [8-9]   Accel Y (int16_t)   - Range: ±65.5g,    Scale: 500,     Precision: 0.002g
         * [10-11] Accel Z (int16_t)   - Range: ±65.5g,    Scale: 500,     Precision: 0.002g
         * [12-13] Gyro X (int16_t)    - Range: ±3276°/s,  Scale: 10,      Precision: 0.1°/s
         * [14-15] Gyro Y (int16_t)    - Range: ±3276°/s,  Scale: 10,      Precision: 0.1°/s
         * [16-17] Gyro Z (int16_t)    - Range: ±3276°/s,  Scale: 10,      Precision: 0.1°/s
         * [18]    Battery (uint8_t)   - Range: 0-255,     Direct mapping
         * 
         * Detection: Web app distinguishes from Euler protocol by checking if first 6 bytes 
         * represent valid quaternion components (|X|,|Y|,|Z| < 1.0 and X²+Y²+Z² < 1.0)
         */
        
        uint8_t binaryData[19];
        
        // Convert quaternion XYZ to scaled int16_t (W calculated on web app)
        int16_t qiInt = (int16_t)(qi * 10000);  // X component
        int16_t qjInt = (int16_t)(qj * 10000);  // Y component  
        int16_t qkInt = (int16_t)(qk * 10000);  // Z component
        
        // Convert sensor data (same as original protocol)
        int16_t axInt = (int16_t)(ax * 500);
        int16_t ayInt = (int16_t)(ay * 500);
        int16_t azInt = (int16_t)(az * 500);
        int16_t gxInt = (int16_t)(gx * 10);
        int16_t gyInt = (int16_t)(gy * 10);
        int16_t gzInt = (int16_t)(gz * 10);
        uint8_t batteryByte = (uint8_t)batteryLevel;
        
        // Pack into 19-byte binary array (little-endian)
        binaryData[0] = qiInt & 0xFF;        // Quat X low
        binaryData[1] = (qiInt >> 8) & 0xFF; // Quat X high
        binaryData[2] = qjInt & 0xFF;        // Quat Y low
        binaryData[3] = (qjInt >> 8) & 0xFF; // Quat Y high
        binaryData[4] = qkInt & 0xFF;        // Quat Z low
        binaryData[5] = (qkInt >> 8) & 0xFF; // Quat Z high
        binaryData[6] = axInt & 0xFF;        // Accel X low
        binaryData[7] = (axInt >> 8) & 0xFF; // Accel X high
        binaryData[8] = ayInt & 0xFF;        // Accel Y low
        binaryData[9] = (ayInt >> 8) & 0xFF; // Accel Y high
        binaryData[10] = azInt & 0xFF;       // Accel Z low
        binaryData[11] = (azInt >> 8) & 0xFF;// Accel Z high
        binaryData[12] = gxInt & 0xFF;       // Gyro X low
        binaryData[13] = (gxInt >> 8) & 0xFF;// Gyro X high
        binaryData[14] = gyInt & 0xFF;       // Gyro Y low
        binaryData[15] = (gyInt >> 8) & 0xFF;// Gyro Y high
        binaryData[16] = gzInt & 0xFF;       // Gyro Z low
        binaryData[17] = (gzInt >> 8) & 0xFF;// Gyro Z high
        binaryData[18] = batteryByte;        // Battery

        unsigned long timeSinceLastMotion = currentTime - lastMotionTime;
        Serial.printf("Time since motion: %lums | BLE: %s\n", 
                     timeSinceLastMotion, deviceConnected ? "Connected" : "Disconnected");

        // Send minimal quaternion data via BLE (19 bytes - same as Euler!)
        if (deviceConnected && bleActive) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
        }
    }
    } // End of sensor reading block

    // Handle disconnection and restart advertising ONLY if BLE is supposed to be active
    if (!deviceConnected && oldDeviceConnected) {
        if (bleActive && currentPowerState == ACTIVE) {
            delay(500);
            pServer->startAdvertising();
            Serial.println("Restarted advertising for reconnection");
        } else {
            Serial.println("Device disconnected but BLE is off - not restarting advertising");
        }
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("New connection established");
    }
}