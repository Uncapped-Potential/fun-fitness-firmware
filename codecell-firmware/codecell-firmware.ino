/*
 * CodeCell BLE Quaternion Streamer - Minimal 19-byte Protocol
 * Sends 3-component quaternion data, W component calculated on device
 * Same packet size as original Euler protocol for maximum battery efficiency
 */

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

CodeCell myCodeCell;

// BLE UUIDs (matching MicroLink for compatibility)
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

int dataRate = 24; // 24Hz for battery optimization

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
    Serial.println("CodeCell BLE Minimal Quaternion Streamer - 19 bytes, Gimbal Lock Free!");

    // Initialize all motion sensors including magnetometer
    myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER);

    // Initialize BLE
    BLEDevice::init("FitChip001");
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
}

void loop() {
    if (myCodeCell.Run(dataRate)) {
        // Read quaternion data directly from BNO085 (no gimbal lock!)
        float qr, qi, qj, qk;
        myCodeCell.Motion_RotationVectorRead(qr, qi, qj, qk);
        
        // Read accelerometer and gyro data
        float ax, ay, az;
        float gx, gy, gz;
        myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        myCodeCell.Motion_GyroRead(gx, gy, gz);

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

        // Send minimal quaternion data via BLE (19 bytes - same as Euler!)
        if (deviceConnected) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
            
            // Debug output
            Serial.printf("Quat: [%.3f, %.3f, %.3f, %.3f] -> Send: [%.3f, %.3f, %.3f]\n", 
                         qr, qi, qj, qk, qi, qj, qk);
        }
    }

    // Handle disconnection and restart advertising
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