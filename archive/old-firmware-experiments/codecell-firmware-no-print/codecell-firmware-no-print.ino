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
#include <cmath>
#include <string>
#include "ota_library.h"

CodeCell myCodeCell;

// Forward declarations
// OTA functionality now handled by ota_library

// RTC memory to survive light sleep (deep sleep removed due to BNO085 issues)
RTC_DATA_ATTR bool sensorInitialized = false;
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t lastResetReason = 0;

// BLE UUIDs (matching MicroLink for compatibility)
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"
#define CONFIG_CHAR_UUID    "dcba4330-dcba-4321-dcba-432123456792"  // Configuration characteristic

// OTA functionality moved to ota_library.h/.cpp

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pConfigCharacteristic = NULL;  // Config characteristic
bool deviceConnected = false;
bool oldDeviceConnected = false;

// OTA instance - isolated from main firmware
CodeCellOTA otaManager;

// Power loss recovery and BLE state management
bool bleInitialized = false;
bool sensorsReady = false;
unsigned long lastBleCheck = 0;
unsigned long startupTime = 0;

// Power management configuration
int dataRate = 60; // 60Hz for normal operation - configurable via BLE
int lowPowerDataRate = 1; // 1Hz for low power mode
int imuRate = 60; // IMU polling rate (matches dataRate for normal operation)
int lowPowerImuRate = 10; // Reduced IMU rate for power saving (still responsive for motion detection)
float motionThreshold = 0.4; // Minimum change in acceleration to detect motion (g-force)
int consecutiveMotionRequired = 2; // Require consecutive motion readings to confirm real motion
int bleInactivityTimeout = 10000; // Reduce BLE rate after 10 seconds of no motion
int lightSleepTimeout = 60000; // Enter light sleep (sensor only) after 1 minute

// Configurable parameters
bool removeGravity = false; // Toggle gravity removal - configurable via BLE

// Safety flags for configuration
bool systemInitialized = false; // Prevent config during initialization
bool configInProgress = false;  // Prevent concurrent config operations

// Command IDs for configuration
enum ConfigCommands {
    CMD_SET_GRAVITY = 0x01,    // Toggle gravity removal (0=raw, 1=removed)
    CMD_SET_DATA_RATE = 0x02,  // Set data rate in Hz (10-100)
    CMD_SET_SLEEP_TIMEOUT = 0x03, // Set sleep timeout in seconds
    CMD_GET_CONFIG = 0x04      // Request current config
};

// Deep sleep removed - it completely breaks BNO085 sensor functionality
// Even with RST pin tied to 3.3V, the sensor loses all configuration during ESP32 deep sleep
// and cannot be reinitialized, returning only zeros. Light sleep preserves sensor state.
unsigned long lastMotionTime = 0;
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

// USB power detection
bool isUsbPowered = false;
unsigned long lastPowerCheck = 0;

// OTA implementation moved to ota_library.h/.cpp

// Callback for handling configuration writes
class ConfigCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // Safety checks
        if (!systemInitialized) {

            return;
        }
        
        if (configInProgress) {

            return;
        }
        
        configInProgress = true; // Lock configuration
        
        String arduinoString = pCharacteristic->getValue();
        std::string value(arduinoString.c_str(), arduinoString.length());
        
        if (value.length() > 0) {
            uint8_t cmd = value[0];
            
            switch (cmd) {
                case CMD_SET_GRAVITY:
                    if (value.length() >= 2) {
                        removeGravity = value[1] > 0;


                        
                        // Send acknowledgment back
                        uint8_t ack[2] = {CMD_SET_GRAVITY, removeGravity};
                        pCharacteristic->setValue(ack, 2);
                        pCharacteristic->notify();
                    }
                    break;
                    
                case CMD_SET_DATA_RATE:
                    if (value.length() >= 2) {
                        int newRate = value[1];
                        if (newRate >= 10 && newRate <= 100) {
                            dataRate = newRate;
                            imuRate = newRate; // Keep IMU rate synced



                            
                            // Send acknowledgment
                            uint8_t ack[2] = {CMD_SET_DATA_RATE, (uint8_t)dataRate};
                            pCharacteristic->setValue(ack, 2);
                            pCharacteristic->notify();
                        }
                    }
                    break;
                    
                case CMD_SET_SLEEP_TIMEOUT:
                    if (value.length() >= 2) {
                        int timeoutSec = value[1];
                        if (timeoutSec >= 5 && timeoutSec <= 255) {
                            bleInactivityTimeout = timeoutSec * 1000;
                            // Reset motion timer now that system is safely initialized
                            lastMotionTime = millis();



                            
                            // Send acknowledgment
                            uint8_t ack[2] = {CMD_SET_SLEEP_TIMEOUT, (uint8_t)timeoutSec};
                            pCharacteristic->setValue(ack, 2);
                            pCharacteristic->notify();
                        }
                    }
                    break;
                    
                case CMD_GET_CONFIG:
                    // Send current configuration
                    uint8_t config[5] = {
                        CMD_GET_CONFIG,
                        removeGravity,
                        (uint8_t)dataRate,
                        (uint8_t)(bleInactivityTimeout / 1000),
                        0  // Reserved for future use
                    };
                    pCharacteristic->setValue(config, 5);
                    pCharacteristic->notify();

                    break;
            }
        }
        
        configInProgress = false; // Unlock configuration
    }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;

    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;

    }
};

// Robust BLE initialization with recovery logic
bool initializeBLE() {

    
    try {
        // Initialize BLE device
        BLEDevice::init("FitChip011");
        
        // Create server with callbacks
        pServer = BLEDevice::createServer();
        if (!pServer) {

            return false;
        }
        pServer->setCallbacks(new MyServerCallbacks());

        // Create main service
        BLEService *pService = pServer->createService(SERVICE_UUID);
        if (!pService) {

            return false;
        }
        
        // Create data characteristic
        pCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID,
                          BLECharacteristic::PROPERTY_READ |
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
        if (!pCharacteristic) {

            return false;
        }
        pCharacteristic->addDescriptor(new BLE2902());
        
        // Create configuration characteristic
        pConfigCharacteristic = pService->createCharacteristic(
                              CONFIG_CHAR_UUID,
                              BLECharacteristic::PROPERTY_READ |
                              BLECharacteristic::PROPERTY_WRITE |
                              BLECharacteristic::PROPERTY_NOTIFY
                            );
        if (!pConfigCharacteristic) {

            return false;
        }
        pConfigCharacteristic->setCallbacks(new ConfigCallbacks());
        pConfigCharacteristic->addDescriptor(new BLE2902());
        
        // Set initial config values
        uint8_t initialConfig[5] = {CMD_GET_CONFIG, removeGravity, (uint8_t)dataRate, (uint8_t)(bleInactivityTimeout / 1000), 0};
        pConfigCharacteristic->setValue(initialConfig, 5);
        
        // Start the service
        pService->start();
        
        // Initialize OTA service
        if (!otaManager.init(pServer)) {

        }

        // Setup advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        if (!pAdvertising) {

            return false;
        }
        
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->addServiceUUID("8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001"); // OTA service
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0);
        
        // Start advertising with error handling
        BLEDevice::startAdvertising();
        

        return true;
        
    } catch (const std::exception& e) {

        return false;
    } catch (...) {

        return false;
    }
}

// Function to recover BLE after power issues
bool recoverBLE() {

    
    // Clean up existing BLE state
    if (pServer && pServer->getAdvertising()->isAdvertising()) {
        pServer->getAdvertising()->stop();
        delay(100);
    }
    
    // Deinitialize and reinitialize
    BLEDevice::deinit(false);
    delay(500);
    
    return initializeBLE();
}

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

        return false; // No motion on first reading
    }
    
    // Calculate change in acceleration from previous reading
    float deltaAx = fabsf(ax - lastAx);
    float deltaAy = fabsf(ay - lastAy);
    float deltaAz = fabsf(az - lastAz);
    float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
    

    
    // Update baseline for next comparison
    lastAx = ax;
    lastAy = ay;
    lastAz = az;
    
    if (totalDelta > motionThreshold) {
        consecutiveMotionCount++;

        
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
    // Serial removed for performance testing
    delay(1000);
    




    
    // Track boot count and initialization state
    bootCount++;
    startupTime = millis();
    
    // Power management: NO DEEP SLEEP due to BNO085 compatibility issues

    
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {

        sensorInitialized = false;
    } else {

    }
    
    // Initialize sensors with robust error handling

    delay(2000);
    
    bool sensorsOK = false;
    for (int attempt = 1; attempt <= 3; attempt++) {

        
        myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);
        delay(1000);
        
        float testAx, testAy, testAz;
        float testQr, testQi, testQj, testQk;
        myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
        myCodeCell.Motion_RotationVectorRead(testQr, testQi, testQj, testQk);
        
        bool accelWorking = (testAx != 0.0 || testAy != 0.0 || testAz != 0.0);
        
        // Sensor test removed for performance
        
        if (accelWorking) {

            sensorsOK = true;
            sensorInitialized = true;
            sensorsReady = true;
            break;
        } else {

            if (attempt < 3) {
                delay(2000);
            }
        }
    }
    
    if (!sensorsOK) {


    }
    
    // Initialize BLE with retry logic

    
    int bleAttempts = 0;
    while (!bleInitialized && bleAttempts < 3) {
        bleAttempts++;

        
        if (initializeBLE()) {
            bleInitialized = true;

            break;
        } else {

            if (bleAttempts < 3) {
                delay(2000);
            }
        }
    }
    
    if (!bleInitialized) {


    }
    
    // Final setup
    lastMotionTime = millis();
    lastBleCheck = millis();
    lastPowerCheck = millis();
    
    systemInitialized = true;
    


    // Status output removed for performance






}

// OTA service setup moved to ota_library.h/.cpp

void loop() {
    unsigned long currentTime = millis();
    
    // Check for BLE recovery needs
    if (bleInitialized && currentTime - lastBleCheck > 10000) {
        lastBleCheck = currentTime;
        
        // Verify BLE is still functional
        if (!pServer || !pServer->getAdvertising()) {

            if (recoverBLE()) {

            } else {

                bleInitialized = false;
            }
        }
    }
    
    // Check USB power status every 5 seconds
    if (currentTime - lastPowerCheck > 5000) {
        lastPowerCheck = currentTime;
        int powerState = myCodeCell.PowerStateRead();
        isUsbPowered = (powerState == 1);
        
        if (isUsbPowered) {

        }
    }
    
    // Power management state machine
    if (currentTime - lastSleepCheck > 1000) {
        lastSleepCheck = currentTime;
        lastBleCheck = currentTime;
        unsigned long timeSinceMotion = currentTime - lastMotionTime;
        
        // Skip power management if USB powered - stay at full performance
        if (isUsbPowered) {
            // Ensure we're in ACTIVE state when USB powered
            if (currentPowerState != ACTIVE) {

                //setCpuFrequencyMhz(160); // Restore full speed
                dataRate = 60; // Restore full data rate
                imuRate = 60; // Restore full IMU rate
                bleActive = true;
                if (bleInitialized && pServer && !pServer->getAdvertising()->isAdvertising()) {
                    pServer->getAdvertising()->start();
                }
                currentPowerState = ACTIVE;
            }
            // Skip the power management state machine when USB powered
        } else {
            // State machine for power management (battery only)
            switch (currentPowerState) {
            case ACTIVE:
                if (!otaManager.isActive() && timeSinceMotion > bleInactivityTimeout) {

                    dataRate = lowPowerDataRate; // Reduce to 1Hz transmission
                    imuRate = lowPowerImuRate; // Reduce IMU polling to 10Hz
                    bleActive = false; // Actually turn off BLE notifications
                    if (bleInitialized && pServer) {
                        pServer->getAdvertising()->stop();
                    }

                    currentPowerState = BLE_OFF;
                }
                break;
                
            case BLE_OFF:
                if (timeSinceMotion < 500) {

                    dataRate = 60; // Restore full data rate
                    imuRate = 60; // Restore full IMU rate
                    bleActive = true; // Re-enable BLE
                    if (bleInitialized && pServer) {
                        pServer->getAdvertising()->start();
                    }
                    currentPowerState = ACTIVE;
                } else if (!otaManager.isActive() && timeSinceMotion > lightSleepTimeout) {


                    imuRate = 5; // Reduce to 5Hz for maximum power savings in light sleep
                    currentPowerState = LIGHT_SLEEP;
                    // Keep sensors running, just reduce CPU frequency
                    // setCpuFrequencyMhz(160); // Reduce from 160MHz to 80MHz
                }
                break;
                
            case LIGHT_SLEEP:
                if (timeSinceMotion < 500) {

                    //setCpuFrequencyMhz(160); // Restore full speed
                    dataRate = 60; // Restore full data rate
                    imuRate = 60; // Restore full IMU rate
                    bleActive = true;
                    currentPowerState = ACTIVE;
                }
                // Stay in light sleep - no deeper sleep mode available due to sensor issues
                break;
            }
        }
    }
    
    // Sensor processing with recovery
    if (sensorsReady && myCodeCell.Run(imuRate)) {
        // Read quaternion data directly from BNO085 (no gimbal lock!)
        float qr, qi, qj, qk;
        myCodeCell.Motion_RotationVectorRead(qr, qi, qj, qk);
        
        // Read accelerometer and gyro data
        float ax, ay, az;
        float gx, gy, gz;
        
        // Use configured gravity removal setting
        if (removeGravity) {
            // Use linear acceleration (gravity already removed by sensor fusion)
            myCodeCell.Motion_LinearAccRead(ax, ay, az);
        } else {
            // Use raw accelerometer (includes gravity)
            myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        }
        
        myCodeCell.Motion_GyroRead(gx, gy, gz);

        // Check for motion activity to reset sleep timer using delta method
        float totalDelta = 0.0;
        if (motionBaselineSet) {
            float deltaAx = fabsf(ax - lastAx);
            float deltaAy = fabsf(ay - lastAy);
            float deltaAz = fabsf(az - lastAz);
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

        // Debug output removed for performance

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
        
        // Convert all sensor data to real values
        int16_t axInt = (int16_t)(ax * 500);    // Real accelerometer X
        int16_t ayInt = (int16_t)(ay * 500);    // Real accelerometer Y  
        int16_t azInt = (int16_t)(az * 500);    // Real accelerometer Z
        int16_t gxInt = (int16_t)(gx * 10);     // Real gyroscope X
        int16_t gyInt = (int16_t)(gy * 10);     // Real gyroscope Y
        int16_t gzInt = (int16_t)(gz * 10);     // Real gyroscope Z
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
        // Motion timing output removed for performance

        // Send data via BLE with error handling
        if (bleInitialized && deviceConnected && bleActive && !otaManager.isActive()) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
        } else if (otaManager.isActive()) {
            static unsigned long lastOtaThrottleLog = 0;
            if (currentTime - lastOtaThrottleLog > 5000) {

                lastOtaThrottleLog = currentTime;
            }
        }
    }
    // End of sensor reading block

    // Handle BLE connection management with recovery
    if (bleInitialized) {
        if (!deviceConnected && oldDeviceConnected) {
            if (bleActive && currentPowerState == ACTIVE) {
                delay(500);
                if (pServer) {
                    pServer->startAdvertising();

                }
            } else {

            }
            oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected) {
            oldDeviceConnected = deviceConnected;

        }
    }
}