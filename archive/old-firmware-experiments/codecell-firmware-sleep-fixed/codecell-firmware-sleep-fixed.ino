/*
 * CodeCell BLE Quaternion Streamer with Motion-Based Sleep Mode - POWER MANAGEMENT FIXED
 * 
 * FIX: Power management readings require Run() cycles to stabilize
 * Moved USB pwer detectioon after proper initialization settling period
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

// POWER MANAGEMENT FIX: Add stabilization tracking
bool powerReadingsStable = false;
unsigned long powerStabilizationStart = 0;
int powerStabilizationCycles = 0;

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

// USB power detection - FIXED: Will be set after power stabilization
bool isUsbPowered = false;
unsigned long lastPowerCheck = 0;

// OTA implementation moved to ota_library.h/.cpp

// Callback for handling configuration writes
class ConfigCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // Safety checks
        if (!systemInitialized) {
            Serial.println("CONFIG: System not initialized, ignoring command");
            return;
        }
        
        if (configInProgress) {
            Serial.println("CONFIG: Configuration in progress, ignoring command");
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
                        Serial.print("Gravity removal set to: ");
                        Serial.println(removeGravity ? "ON" : "OFF");
                        
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
                            Serial.print("Data rate set to: ");
                            Serial.print(dataRate);
                            Serial.println(" Hz");
                            
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
                            Serial.print("Sleep timeout set to: ");
                            Serial.print(timeoutSec);
                            Serial.println(" seconds (motion timer reset)");
                            
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
                    Serial.println("Configuration sent to client");
                    break;
            }
        }
        
        configInProgress = false; // Unlock configuration
    }
};

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

// Robust BLE initialization with recovery logic
bool initializeBLE() {
    Serial.println("Initializing BLE system...");
    
    try {
        // Initialize BLE device
        BLEDevice::init("FitChip011");
        
        // Create server with callbacks
        pServer = BLEDevice::createServer();
        if (!pServer) {
            Serial.println("ERROR: Failed to create BLE server");
            return false;
        }
        pServer->setCallbacks(new MyServerCallbacks());

        // Create main service
        BLEService *pService = pServer->createService(SERVICE_UUID);
        if (!pService) {
            Serial.println("ERROR: Failed to create main BLE service");
            return false;
        }
        
        // Create data characteristic
        pCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID,
                          BLECharacteristic::PROPERTY_READ |
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
        if (!pCharacteristic) {
            Serial.println("ERROR: Failed to create data characteristic");
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
            Serial.println("ERROR: Failed to create config characteristic");
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
            Serial.println("WARNING: OTA service initialization failed - continuing without OTA");
        }

        // Setup advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        if (!pAdvertising) {
            Serial.println("ERROR: Failed to get BLE advertising");
            return false;
        }
        
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->addServiceUUID("8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001"); // OTA service
        pAdvertising->setScanResponse(false);
        pAdvertising->setMinPreferred(0x0);
        
        // Start advertising with error handling
        BLEDevice::startAdvertising();
        
        Serial.println("BLE initialization completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        Serial.printf("BLE initialization exception: %s\n", e.what());
        return false;
    } catch (...) {
        Serial.println("BLE initialization failed with unknown exception");
        return false;
    }
}

// Function to recover BLE after power issues
bool recoverBLE() {
    Serial.println("Attempting BLE recovery...");
    
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

// POWER MANAGEMENT FIX: Stabilize power readings before making decisions
bool stabilizePowerReadings() {
    if (powerReadingsStable) {
        return true;
    }
    
    powerStabilizationCycles++;
    
    // Read current power state
    uint8_t powerState = myCodeCell.PowerStateRead();
    uint8_t batteryLevel = myCodeCell.BatteryLevelRead();
    
    Serial.printf("Power stabilization cycle %d: PowerState=%d, BatteryLevel=%d\n", 
                  powerStabilizationCycles, powerState, batteryLevel);
    
    // Consider stable when _charge_state has been set (BatteryLevel no longer 255)
    // and PowerState is no longer initializing (not 2)
    if (powerState != 2 && batteryLevel != 255) {
        powerReadingsStable = true;
        
        // NOW we can safely determine USB power state
        isUsbPowered = (powerState == 1);
        
        Serial.printf("✅ Power readings stabilized after %d cycles\n", powerStabilizationCycles);
        Serial.printf("✅ PowerState=%d, BatteryLevel=%d\n", powerState, batteryLevel);
        Serial.printf("✅ USB power detection: %s\n", isUsbPowered ? "USB POWERED" : "BATTERY POWERED");
        
        return true;
    }
    
    // Safety timeout - don't wait forever
    if (powerStabilizationCycles > 20) {
        Serial.println("⚠️ Power stabilization timeout - proceeding anyway");
        powerReadingsStable = true;
        isUsbPowered = false; // Default to battery mode
        return true;
    }
    
    return false;
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
        Serial.printf("Motion baseline set: [%.3f, %.3f, %.3f]\n", ax, ay, az);
        return false; // No motion on first reading
    }
    
    // Calculate change in acceleration from previous reading
    float deltaAx = fabsf(ax - lastAx);
    float deltaAy = fabsf(ay - lastAy);
    float deltaAz = fabsf(az - lastAz);
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
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("CodeCell Sleep Mode Firmware v2.1");
    Serial.println("POWER MANAGEMENT FIXED VERSION");
    Serial.println("=================================");
    
    // Track boot count and initialization state
    bootCount++;
    startupTime = millis();
    
    // Power management: NO DEEP SLEEP due to BNO085 compatibility issues
    Serial.printf("Boot #%d - Initializing system...\n", bootCount);
    
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        Serial.println("Fresh boot - full initialization required");
        sensorInitialized = false;
    } else {
        Serial.println("Wake from light sleep - checking sensor state");
    }
    
    // Initialize sensors with robust error handling
    Serial.println("Initializing motion sensors...");
    delay(2000);
    
    bool sensorsOK = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("Sensor initialization attempt %d/3...\n", attempt);
        
        myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);
        delay(1000);
        
        float testAx, testAy, testAz;
        float testQr, testQi, testQj, testQk;
        myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
        myCodeCell.Motion_RotationVectorRead(testQr, testQi, testQj, testQk);
        
        bool accelWorking = (testAx != 0.0 || testAy != 0.0 || testAz != 0.0);
        
        Serial.printf("Sensor test: Accel=[%.3f, %.3f, %.3f], Quat=[%.3f, %.3f, %.3f, %.3f]\n", 
                      testAx, testAy, testAz, testQr, testQi, testQj, testQk);
        
        if (accelWorking) {
            Serial.println("Sensors initialized successfully");
            sensorsOK = true;
            sensorInitialized = true;
            sensorsReady = true;
            break;
        } else {
            Serial.printf("Sensor initialization failed (attempt %d)\n", attempt);
            if (attempt < 3) {
                delay(2000);
            }
        }
    }
    
    if (!sensorsOK) {
        Serial.println("CRITICAL: Sensor initialization failed after 3 attempts");
        Serial.println("System will continue but may have limited functionality");
    }
    
    // POWER MANAGEMENT FIX: Start power stabilization process
    Serial.println("Starting power management stabilization...");
    powerStabilizationStart = millis();
    
    // Initialize BLE with retry logic
    Serial.println("Initializing BLE system...");
    
    int bleAttempts = 0;
    while (!bleInitialized && bleAttempts < 3) {
        bleAttempts++;
        Serial.printf("BLE initialization attempt %d/3...\n", bleAttempts);
        
        if (initializeBLE()) {
            bleInitialized = true;
            Serial.println("BLE system ready");
            break;
        } else {
            Serial.printf("BLE initialization failed (attempt %d)\n", bleAttempts);
            if (bleAttempts < 3) {
                delay(2000);
            }
        }
    }
    
    if (!bleInitialized) {
        Serial.println("CRITICAL: BLE initialization failed after 3 attempts");
        Serial.println("Device will not be discoverable - manual reset required");
    }
    
    // Final setup - but USB detection happens in loop after stabilization
    lastMotionTime = millis();
    lastBleCheck = millis();
    lastPowerCheck = millis();
    
    Serial.println("=================================");
    Serial.printf("Initialization complete in %lums\n", millis() - startupTime);
    Serial.printf("Sensors: %s | BLE: %s\n", 
                  sensorsReady ? "OK" : "FAILED", 
                  bleInitialized ? "OK" : "FAILED");
    Serial.println("WAITING FOR POWER STABILIZATION...");
    Serial.println("=================================");
}

void loop() {
    unsigned long currentTime = millis();
    
    // POWER MANAGEMENT FIX: Stabilize power readings first
    if (!powerReadingsStable) {
        if (myCodeCell.Run(5)) {  // Run at 5Hz during stabilization
            stabilizePowerReadings();
        }
        return; // Don't proceed until power is stable
    }
    
    // Mark system as initialized only after power stabilization
    if (!systemInitialized) {
        systemInitialized = true;
        Serial.println("✅ System fully initialized with stable power readings");
    }
    
    // Check for BLE recovery needs
    if (bleInitialized && currentTime - lastBleCheck > 10000) {
        lastBleCheck = currentTime;
        
        // Verify BLE is still functional
        if (!pServer || !pServer->getAdvertising()) {
            Serial.println("BLE system appears corrupted - attempting recovery");
            if (recoverBLE()) {
                Serial.println("BLE recovery successful");
            } else {
                Serial.println("BLE recovery failed - continuing without BLE");
                bleInitialized = false;
            }
        }
    }
    
    // Check USB power status every 5 seconds (now that readings are stable)
    if (currentTime - lastPowerCheck > 5000) {
        lastPowerCheck = currentTime;
        int powerState = myCodeCell.PowerStateRead();
        bool currentUsbState = (powerState == 1);
        
        if (currentUsbState != isUsbPowered) {
            Serial.printf("USB power state changed: %s -> %s\n", 
                         isUsbPowered ? "USB" : "BATTERY",
                         currentUsbState ? "USB" : "BATTERY");
            isUsbPowered = currentUsbState;
        }
        
        if (isUsbPowered) {
            Serial.println("USB powered - maintaining 60Hz operation");
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
                Serial.println("USB detected - returning to ACTIVE state");
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
                    Serial.printf("Entering BLE_OFF state (1Hz mode) after %lums\n", timeSinceMotion);
                    dataRate = lowPowerDataRate; // Reduce to 1Hz transmission
                    imuRate = lowPowerImuRate; // Reduce IMU polling to 10Hz
                    bleActive = false; // Actually turn off BLE notifications
                    if (bleInitialized && pServer) {
                        pServer->getAdvertising()->stop();
                    }
                    Serial.println("BLE_OFF: BLE disabled, IMU polling at 10Hz");
                    currentPowerState = BLE_OFF;
                }
                break;
                
            case BLE_OFF:
                if (timeSinceMotion < 500) {
                    Serial.printf("Motion detected - returning to ACTIVE state\n");
                    dataRate = 60; // Restore full data rate
                    imuRate = 60; // Restore full IMU rate
                    bleActive = true; // Re-enable BLE
                    if (bleInitialized && pServer) {
                        pServer->getAdvertising()->start();
                    }
                    currentPowerState = ACTIVE;
                } else if (!otaManager.isActive() && timeSinceMotion > lightSleepTimeout) {
                    Serial.printf("Entering LIGHT_SLEEP state after %lums\n", timeSinceMotion);
                    Serial.println("LIGHT_SLEEP: Sensors active, BLE off, IMU at 5Hz");
                    imuRate = 5; // Reduce to 5Hz for maximum power savings in light sleep
                    currentPowerState = LIGHT_SLEEP;
                    // Keep sensors running, no CPU frequency changes
                }
                break;
                
            case LIGHT_SLEEP:
                if (timeSinceMotion < 500) {
                    Serial.printf("Motion detected - waking to ACTIVE state\n");
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
        Serial.printf("Time since motion: %lums | BLE: %s\n", 
                     timeSinceLastMotion, deviceConnected ? "Connected" : "Disconnected");

        // Send data via BLE with error handling
        if (bleInitialized && deviceConnected && bleActive && !otaManager.isActive()) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
        } else if (otaManager.isActive()) {
            static unsigned long lastOtaThrottleLog = 0;
            if (currentTime - lastOtaThrottleLog > 5000) {
                Serial.println("IMU streaming throttled during OTA");
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
                    Serial.println("Restarted advertising for reconnection");
                }
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
}