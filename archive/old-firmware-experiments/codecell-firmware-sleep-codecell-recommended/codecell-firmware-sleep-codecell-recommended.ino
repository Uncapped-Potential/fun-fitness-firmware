/*
 * CodeCell BLE Quaternion Streamer - CodeCell Team Recommended Implementation
 * Based on feedback from CodeCell team and their motion alarm example
 * 
 * Key Changes:
 * - Uses myCodeCell.Sleep(1) instead of custom ESP32 light sleep
 * - Uses myCodeCell.WakeUpCheck() for proper wake-up handling  
 * - Single myCodeCell.Run() call per loop as recommended
 * - Proper motion sensor initialization sequence
 * - Follows CodeCell team's recommended patterns
 */

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <cmath>
#include <string>

CodeCell myCodeCell;

// BLE UUIDs (matching MicroLink for compatibility)
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"
#define CONFIG_CHAR_UUID    "dcba4330-dcba-4321-dcba-432123456792"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pConfigCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Power management configuration
int dataRate = 60; // 60Hz for normal operation - configurable via BLE
float motionThreshold = 0.4; // Minimum change in acceleration to detect motion
int bleInactivityTimeout = 10000; // 10 seconds of no motion before considering sleep
int deepSleepTimeout = 60000; // 1 minute of no motion before deep sleep

// Configurable parameters
bool removeGravity = false; // Toggle gravity removal - configurable via BLE

// Safety flags for configuration
bool systemInitialized = false; // Prevent config during initialization

// Command IDs for configuration
enum ConfigCommands {
    CMD_SET_GRAVITY = 0x01,    // Toggle gravity removal (0=raw, 1=removed)
    CMD_SET_DATA_RATE = 0x02,  // Set data rate in Hz (10-100)
    CMD_SET_SLEEP_TIMEOUT = 0x03, // Set sleep timeout in seconds
    CMD_GET_CONFIG = 0x04      // Request current config
};

// Motion detection variables
float lastAx = 0, lastAy = 0, lastAz = 0;
bool motionBaselineSet = false;
unsigned long lastMotionTime = 0;
unsigned long lastPowerCheck = 0;
bool isUsbPowered = false;

// Callback for handling configuration writes
class ConfigCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // Safety checks
        if (!systemInitialized) {
            Serial.println("CONFIG: System not initialized, ignoring command");
            return;
        }
        
        String arduinoString = pCharacteristic->getValue();
        std::string value(arduinoString.c_str(), arduinoString.length());
        if (value.length() > 0) {
            uint8_t command = value[0];
            Serial.printf("CONFIG: Received command 0x%02X\n", command);
            
            switch (command) {
                case CMD_SET_GRAVITY:
                    if (value.length() >= 2) {
                        removeGravity = (value[1] != 0);
                        Serial.printf("CONFIG: Gravity removal set to %s\n", removeGravity ? "enabled" : "disabled");
                    }
                    break;
                    
                case CMD_SET_DATA_RATE:
                    if (value.length() >= 2) {
                        int newRate = value[1];
                        if (newRate >= 10 && newRate <= 100) {
                            dataRate = newRate;
                            Serial.printf("CONFIG: Data rate set to %d Hz\n", dataRate);
                        }
                    }
                    break;
                    
                case CMD_SET_SLEEP_TIMEOUT:
                    if (value.length() >= 2) {
                        int newTimeout = value[1];
                        if (newTimeout >= 5 && newTimeout <= 255) {
                            deepSleepTimeout = newTimeout * 1000; // Convert to milliseconds
                            Serial.printf("CONFIG: Sleep timeout set to %d seconds\n", newTimeout);
                        }
                    }
                    break;
                    
                case CMD_GET_CONFIG:
                    {
                        uint8_t config[5] = {
                            CMD_GET_CONFIG,
                            removeGravity ? 1 : 0,
                            (uint8_t)dataRate,
                            (uint8_t)(deepSleepTimeout / 1000),
                            0
                        };
                        pCharacteristic->setValue(config, 5);
                        pCharacteristic->notify();
                        Serial.println("CONFIG: Sent current configuration");
                    }
                    break;
                    
                default:
                    Serial.printf("CONFIG: Unknown command 0x%02X\n", command);
                    break;
            }
        }
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("CodeCell Motion Streamer v3.0");
    Serial.println("CodeCell Team Recommended Implementation");
    Serial.println("=================================");
    
    // Check wake-up cause using CodeCell's recommended method
    bool wokeFromSleep = myCodeCell.WakeUpCheck();
    
    if (wokeFromSleep) {
        Serial.println("Device woke from sleep - checking motion state");
        
        // Initialize motion sensor as recommended by CodeCell team
        myCodeCell.Motion_Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + 
                              MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);
        
        // Wait for sensor to stabilize and read motion state
        delay(100);
        myCodeCell.Motion_Read();
        
        // Check if motion detected - if not, go back to sleep
        float ax, ay, az;
        myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        
        // Set baseline for motion detection
        lastAx = ax; lastAy = ay; lastAz = az;
        motionBaselineSet = true;
        
        // Check for significant motion
        bool motionDetected = (abs(ax) > 1.0 || abs(ay) > 1.0 || abs(az - 9.8) > 2.0);
        
        if (!motionDetected) {
            Serial.println("No significant motion detected - returning to sleep");
            myCodeCell.Sleep(1); // Sleep for 1 second and check again
        }
        
        Serial.println("Motion detected - staying awake for BLE streaming");
        lastMotionTime = millis();
        
    } else {
        Serial.println("Fresh boot - full initialization");
        
        // Full sensor initialization for fresh boot
        Serial.println("Initializing motion sensors...");
        
        // Use CodeCell's recommended single Init call
        myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + 
                       MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);
        
        // Give sensors time to stabilize (CodeCell team noted fusion algorithms need longer delay)
        delay(2000);
        
        // Test sensor functionality
        float testAx, testAy, testAz;
        myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
        
        Serial.printf("Sensor test: Accel=[%.3f, %.3f, %.3f]\n", testAx, testAy, testAz);
        
        if (testAx == 0.0 && testAy == 0.0 && testAz == 0.0) {
            Serial.println("WARNING: Sensor initialization may have failed");
            // Continue anyway - sensors might need more time
        } else {
            Serial.println("Sensors initialized successfully");
        }
        
        // Set motion baseline
        lastAx = testAx; lastAy = testAy; lastAz = testAz;
        motionBaselineSet = true;
        lastMotionTime = millis();
    }
    
    // Initialize BLE
    Serial.println("Initializing BLE system...");
    
    BLEDevice::init("FitChip011");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Data characteristic
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
    pCharacteristic->addDescriptor(new BLE2902());
    
    // Configuration characteristic
    pConfigCharacteristic = pService->createCharacteristic(
                          CONFIG_CHAR_UUID,
                          BLECharacteristic::PROPERTY_READ |
                          BLECharacteristic::PROPERTY_WRITE |
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
    pConfigCharacteristic->setCallbacks(new ConfigCallbacks());
    pConfigCharacteristic->addDescriptor(new BLE2902());
    
    // Set initial config values
    uint8_t initialConfig[5] = {CMD_GET_CONFIG, removeGravity, (uint8_t)dataRate, (uint8_t)(deepSleepTimeout / 1000), 0};
    pConfigCharacteristic->setValue(initialConfig, 5);
    
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    systemInitialized = true;
    
    Serial.println("System ready - BLE advertising started");
    Serial.println("Configuration commands available via BLE");
    Serial.println("=================================");
}

void loop() {
    // Single Run() call as recommended by CodeCell team
    if (myCodeCell.Run(dataRate)) {
        
        // Check USB power status periodically
        unsigned long currentTime = millis();
        if (currentTime - lastPowerCheck > 5000) {
            lastPowerCheck = currentTime;
            int powerState = myCodeCell.PowerStateRead();
            isUsbPowered = (powerState == 1);
        }
        
        // Read quaternion data
        float qr, qi, qj, qk;
        myCodeCell.Motion_RotationVectorRead(qr, qi, qj, qk);
        
        // Read accelerometer and gyro data
        float ax, ay, az;
        float gx, gy, gz;
        
        // Use configured gravity removal setting
        if (removeGravity) {
            // Use linear acceleration (gravity removed by sensor fusion)
            myCodeCell.Motion_LinearAccRead(ax, ay, az);
        } else {
            // Use raw accelerometer (includes gravity)
            myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        }
        
        myCodeCell.Motion_GyroRead(gx, gy, gz);
        
        // Motion detection for sleep management
        if (motionBaselineSet) {
            float deltaAx = abs(ax - lastAx);
            float deltaAy = abs(ay - lastAy);
            float deltaAz = abs(az - lastAz);
            float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
            
            if (totalDelta > motionThreshold) {
                lastMotionTime = currentTime;
            }
            
            // Update baseline
            lastAx = ax; lastAy = ay; lastAz = az;
        }

        // Read battery level
        int batteryLevel = myCodeCell.BatteryLevelRead();

        // Pack quaternion data into minimal binary format (19 bytes)
        uint8_t binaryData[19];
        
        // Convert to scaled integers
        int16_t qiInt = (int16_t)(qi * 10000);  // X component
        int16_t qjInt = (int16_t)(qj * 10000);  // Y component  
        int16_t qkInt = (int16_t)(qk * 10000);  // Z component
        int16_t axInt = (int16_t)(ax * 500);
        int16_t ayInt = (int16_t)(ay * 500);
        int16_t azInt = (int16_t)(az * 500);
        int16_t gxInt = (int16_t)(gx * 10);
        int16_t gyInt = (int16_t)(gy * 10);
        int16_t gzInt = (int16_t)(gz * 10);
        uint8_t batteryByte = (uint8_t)batteryLevel;
        
        // Pack into binary array (little-endian)
        binaryData[0] = qiInt & 0xFF; binaryData[1] = (qiInt >> 8) & 0xFF;
        binaryData[2] = qjInt & 0xFF; binaryData[3] = (qjInt >> 8) & 0xFF;
        binaryData[4] = qkInt & 0xFF; binaryData[5] = (qkInt >> 8) & 0xFF;
        binaryData[6] = axInt & 0xFF; binaryData[7] = (axInt >> 8) & 0xFF;
        binaryData[8] = ayInt & 0xFF; binaryData[9] = (ayInt >> 8) & 0xFF;
        binaryData[10] = azInt & 0xFF; binaryData[11] = (azInt >> 8) & 0xFF;
        binaryData[12] = gxInt & 0xFF; binaryData[13] = (gxInt >> 8) & 0xFF;
        binaryData[14] = gyInt & 0xFF; binaryData[15] = (gyInt >> 8) & 0xFF;
        binaryData[16] = gzInt & 0xFF; binaryData[17] = (gzInt >> 8) & 0xFF;
        binaryData[18] = batteryByte;

        // Send data via BLE
        if (deviceConnected) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
        }
        
        // Power management - use CodeCell's sleep function if no motion and not USB powered
        unsigned long timeSinceMotion = currentTime - lastMotionTime;
        
        if (!isUsbPowered && timeSinceMotion > deepSleepTimeout) {
            Serial.printf("No motion for %lums - entering deep sleep\n", timeSinceMotion);
            Serial.flush();
            
            // Use CodeCell's recommended sleep function
            myCodeCell.Sleep(1); // Sleep for 1 second, then check motion again
        }
    }
    
    // Handle BLE connection management
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Restarted advertising for reconnection");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("New connection established");
    }
}