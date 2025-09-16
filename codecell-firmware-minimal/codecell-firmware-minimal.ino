/*
 * CodeCell Minimal Motion Detection Firmware - Phase 4
 * Performance-optimized modular architecture
 *
 * Achieved performance: A:60Hz G:60Hz Q:20-40Hz BLE:60Hz
 * Maintains vendor patterns while enabling clean module structure
 */

#include "config.h"
#include "imu_manager.h"
#include "ota_worker.h"
#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_ota_ops.h>

CodeCell myCodeCell;
ImuManager imuManager;
OtaWorker otaWorker;

// Debug counters for rate monitoring
static unsigned long blePacketsSent = 0;
static unsigned long lastDebugTime = 0;

// BLE state
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pOtaControlChar = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// LED for visual feedback - use CodeCell's LED_PIN (already defined in library)
// const int LED_PIN = LED_BUILTIN;  // Removed - LED_PIN already defined by CodeCell library

// Simple BLE callbacks - no heavy work in callbacks!
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE Client Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE Client Disconnected");
    }
};

// OTA Control Characteristic Callback
class OtaControlCharCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        String value = pCharacteristic->getValue();

        if (value.length() > 0) {
            const uint8_t* data = (const uint8_t*)value.c_str();
            uint16_t length = value.length();

            Serial.printf("OTA Control: Received %d bytes, command: 0x%02x", length, data[0]);
            // Print hex dump for debugging
            Serial.print(" [");
            for (int i = 0; i < length && i < 16; i++) {
                Serial.printf("%02x", data[i]);
                if (i < length - 1 && i < 15) Serial.print(" ");
            }
            if (length > 16) Serial.print("...");
            Serial.println("]");

            if (length >= 1) {
                uint8_t command = data[0];

                switch (command) {
                    case 0x01: { // OP_START - [0x01][length:4][crc32:4][chunk_size:2] (11 bytes total, little-endian)
                        if (length >= 11) {
                            // Parse little-endian fields
                            uint32_t totalSize = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                            uint32_t expectedCrc32 = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                            uint16_t requestedChunkSize = data[9] | (data[10] << 8);

                            Serial.printf("OTA: START command - Size: %u, CRC32: 0x%08x, Chunk: %u\n",
                                         totalSize, expectedCrc32, requestedChunkSize);

                            if (otaWorker.startOta(totalSize)) {
                                // Send immediate ACK: [0xa1]
                                uint8_t ack_response = 0xa1;
                                pOtaControlChar->setValue(&ack_response, 1);
                                pOtaControlChar->notify();
                                Serial.println("OTA: Sent START ACK: 0xa1");
                            } else {
                                uint8_t error_response = 0xff;
                                pOtaControlChar->setValue(&error_response, 1);
                                pOtaControlChar->notify();
                                Serial.println("OTA: Sent START ERROR: 0xff");
                            }
                        } else {
                            Serial.printf("OTA: START command too short (%d bytes, expected 11)\n", length);
                            uint8_t error_response = 0xff;
                            pOtaControlChar->setValue(&error_response, 1);
                            pOtaControlChar->notify();
                        }
                        break;
                    }

                    case 0x02: { // OP_DATA - [0x02][offset:4][data...] (little-endian)
                        if (length >= 5) {
                            uint32_t offset = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                            uint16_t dataLength = length - 5;
                            const uint8_t* chunkData = &data[5];

                            Serial.printf("OTA Data: Offset %u, Length %u\n", offset, dataLength);

                            if (otaWorker.queueChunk(chunkData, dataLength, offset, false)) {
                                Serial.printf("OTA Data: Chunk at offset %u queued successfully\n", offset);
                            } else {
                                Serial.printf("OTA Data: Failed to queue chunk at offset %u\n", offset);
                            }
                        } else {
                            Serial.printf("OTA: DATA command too short (%d bytes, expected >=5)\n", length);
                        }
                        break;
                    }

                    case 0x03: { // OP_FINISH - [0x03]
                        Serial.println("OTA: FINISH command received");

                        uint8_t finish_ack = 0xa2;
                        pOtaControlChar->setValue(&finish_ack, 1);
                        pOtaControlChar->notify();
                        Serial.println("OTA: Sent FINISH ACK: 0xa2");
                        break;
                    }

                    default: {
                        Serial.printf("OTA: Unknown command: 0x%02x\n", command);
                        uint8_t unknown_response = 0xff;
                        pOtaControlChar->setValue(&unknown_response, 1);
                        pOtaControlChar->notify();
                        break;
                    }
                }
            }
        }
    }
};


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=================================");
    Serial.println("CodeCell Minimal Motion Detection");
    Serial.println("Phase 3: Adding BLE Streaming");
    Serial.println("=================================");

    // Print partition information for OTA debugging
    Serial.println("=== PARTITION INFO ===");
    Serial.printf("Sketch size: %u bytes\n", ESP.getSketchSize());
    Serial.printf("Free sketch space: %u bytes\n", ESP.getFreeSketchSpace());
    Serial.printf("Flash chip size: %u bytes\n", ESP.getFlashChipSize());

    // Check OTA partition size
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition) {
        Serial.printf("OTA partition size: %u bytes\n", update_partition->size);
        Serial.printf("OTA partition address: 0x%x\n", update_partition->address);
    } else {
        Serial.println("No OTA partition found!");
    }

    Serial.println("======================");

    // Setup LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Follow vendor pattern exactly - check wake-up cause
    if (myCodeCell.WakeUpCheck()) {
        Serial.println("Wake-up check passed");
    }

    // Initialize motion sensors - following vendor pattern
    Serial.println("Initializing motion sensors...");

    // Try minimal sensor set first to achieve 60Hz base rate
    // myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);

    // Use performance-optimized sensor configuration from config.h
    Serial.println("Initializing with performance-optimized sensor configuration...");
    myCodeCell.Init(SENSOR_CONFIG);

    // Stabilization delay from config
    delay(SENSOR_STABILIZATION_MS);

    // Verify initialization worked
    float testAx, testAy, testAz;
    myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);

    if (testAx == 0.0 && testAy == 0.0 && testAz == 0.0) {
        Serial.println("ERROR: Accelerometer initialization failed!");
        // Flash LED rapidly to indicate error
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }

    Serial.printf("Initial accel reading: [%.3f, %.3f, %.3f]\n", testAx, testAy, testAz);
    Serial.println("Motion sensor initialization complete");

    // Initialize IMU manager
    if (!imuManager.init()) {
        Serial.println("ERROR: IMU manager initialization failed!");
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }

    // Initialize OTA worker
    if (!otaWorker.init()) {
        Serial.println("ERROR: OTA worker initialization failed!");
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
            delay(50);
        }
    }

    // Initialize BLE
    Serial.println("Initializing BLE...");
    BLEDevice::init(DEVICE_NAME);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Quaternion data characteristic (existing)
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
    pCharacteristic->addDescriptor(new BLE2902());

    // OTA Control characteristic (commands and acknowledgments)
    pOtaControlChar = pService->createCharacteristic(
                      OTA_CONTROL_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
    pOtaControlChar->addDescriptor(new BLE2902());
    pOtaControlChar->setCallbacks(new OtaControlCharCallbacks());


    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    Serial.println("BLE initialization complete - advertising started");
    Serial.println("Starting motion detection and BLE streaming...");
}

void loop() {
    // Follow vendor pattern - single Run() call per loop
    if (myCodeCell.Run(UPDATE_RATE)) {

        // Update IMU manager (all sensor reading and motion detection)
        imuManager.update();
        const ImuManager::SensorData& data = imuManager.getData();

        // Extract values for BLE and debugging
        float qr = data.qr, qi = data.qx, qj = data.qy, qk = data.qz;
        float ax = data.ax, ay = data.ay, az = data.az;
        float gx = data.gx, gy = data.gy, gz = data.gz;

        // Send BLE data if connected (19-byte quaternion protocol)
        if (deviceConnected) {
            uint8_t binaryData[19];

            // Pack quaternion XYZ (W calculated by receiver, sign in battery byte)
            int16_t qiInt = (int16_t)(qi * 10000);
            int16_t qjInt = (int16_t)(qj * 10000);
            int16_t qkInt = (int16_t)(qk * 10000);

            // Pack accelerometer and gyro
            int16_t axInt = (int16_t)(ax * 500);
            int16_t ayInt = (int16_t)(ay * 500);
            int16_t azInt = (int16_t)(az * 500);
            int16_t gxInt = (int16_t)(gx * 10);
            int16_t gyInt = (int16_t)(gy * 10);
            int16_t gzInt = (int16_t)(gz * 10);

            // Pack into binary format (little-endian)
            binaryData[0] = qiInt & 0xFF; binaryData[1] = (qiInt >> 8) & 0xFF;
            binaryData[2] = qjInt & 0xFF; binaryData[3] = (qjInt >> 8) & 0xFF;
            binaryData[4] = qkInt & 0xFF; binaryData[5] = (qkInt >> 8) & 0xFF;
            binaryData[6] = axInt & 0xFF; binaryData[7] = (axInt >> 8) & 0xFF;
            binaryData[8] = ayInt & 0xFF; binaryData[9] = (ayInt >> 8) & 0xFF;
            binaryData[10] = azInt & 0xFF; binaryData[11] = (azInt >> 8) & 0xFF;
            binaryData[12] = gxInt & 0xFF; binaryData[13] = (gxInt >> 8) & 0xFF;
            binaryData[14] = gyInt & 0xFF; binaryData[15] = (gyInt >> 8) & 0xFF;
            binaryData[16] = gzInt & 0xFF; binaryData[17] = (gzInt >> 8) & 0xFF;
            // Read actual battery level and pack W sign in MSB
            uint8_t batteryLevel = myCodeCell.BatteryLevelRead();
            uint8_t wSign = (qr >= 0) ? 0x80 : 0x00;  // Bit 7: 1=positive W, 0=negative W
            binaryData[18] = (batteryLevel & 0x7F) | wSign; // Battery in bits 0-6, W sign in bit 7

            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
            blePacketsSent++; // Count BLE packets sent
        }

        // Motion detection already handled by IMU manager
        bool currentMotion = data.motionDetected;
        if (currentMotion) {
            // Flash LED on motion (non-blocking)
            digitalWrite(LED_PIN, HIGH);
        }

        // Comprehensive debug output every 1 second (faster feedback)
        if (millis() - lastDebugTime >= DEBUG_INTERVAL_MS) {
            unsigned long elapsed = millis() - lastDebugTime;
            lastDebugTime = millis();

            // Get counter updates from IMU manager
            unsigned long accelUpdateCount = 0, gyroUpdateCount = 0, quatUpdateCount = 0;
            imuManager.updateCounters(accelUpdateCount, gyroUpdateCount, quatUpdateCount);

            // Calculate actual rates (Hz)
            float accelHz = (accelUpdateCount * 1000.0) / elapsed;
            float gyroHz = (gyroUpdateCount * 1000.0) / elapsed;
            float quatHz = (quatUpdateCount * 1000.0) / elapsed;
            float bleHz = (blePacketsSent * 1000.0) / elapsed;

            // Compact debug line with all critical info
            Serial.printf("[1s] Rates: A:%.1fHz G:%.1fHz Q:%.1fHz BLE:%.1fHz | Quat:[%.2f,%.2f,%.2f,%.2f] valid:%s | BLE:%s | Heap:%dk\n",
                         accelHz, gyroHz, quatHz, bleHz,
                         qr, qi, qj, qk, data.validQuaternion ? "YES" : "NO",
                         deviceConnected ? "Conn" : "Disc",
                         esp_get_free_heap_size() / 1024);

            // Motion detection status
            if (currentMotion) {
                Serial.printf("*** MOTION: delta=%.3f threshold=%.3f ***\n",
                             data.motionDelta, MOTION_THRESHOLD);
            }

            // OTA status monitoring with detailed state info
            if (otaWorker.isActive()) {
                const OtaWorker::OtaStatus& otaStatus = otaWorker.getStatus();
                const char* stateNames[] = {"IDLE", "RECEIVING", "VALIDATING", "APPLYING", "SUCCESS", "ERROR"};
                const char* stateName = (otaStatus.state < 6) ? stateNames[otaStatus.state] : "UNKNOWN";

                Serial.printf("OTA: %s - %u chunks (%u/%u bytes)",
                             stateName, otaStatus.chunksReceived, otaStatus.bytesReceived, otaStatus.totalSize);

                if (otaStatus.state == 5) { // OTA_ERROR
                    Serial.printf(" - ERROR: %s", otaStatus.errorMessage);
                }
                Serial.println();
            }

            // Reset BLE counter for next measurement
            blePacketsSent = 0;

            // Check if we should sleep (following vendor pattern)
            // Only sleep if not connected to BLE and no OTA in progress
            if (!imuManager.isMotionActive() && !deviceConnected && !otaWorker.isActive()) {
                Serial.println("No motion detected - entering sleep mode");
                Serial.println("Using vendor myCodeCell.Sleep(1) - will wake in 1 second");
                Serial.flush(); // Ensure output before sleep

                // Use vendor sleep function (NOT esp_deep_sleep_start)
                myCodeCell.Sleep(1);  // Sleep for 1 second

                // This code runs after wake-up
                Serial.println("Woke up from sleep - checking motion state");
            }
        }
    }

    // Handle BLE disconnection and restart advertising
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Restarted BLE advertising");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // Minimal delay for cooperative multitasking
    delay(MAIN_LOOP_DELAY_MS);
}