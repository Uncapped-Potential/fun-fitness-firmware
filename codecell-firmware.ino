/*
 * CodeCell Computer BLE IMU Streamer with Auto-Reconnect and Battery LED
 * 
 * Streams clean IMU data and battery level directly to computer GUI via BLE
 * LED indicates battery status with color coding matching web app
 * Automatically restarts advertising if connection is lost
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

int dataRate = 50; // 50Hz default
int currentBatteryLevel = -1; // Cache battery level
unsigned long lastBatteryRead = 0;

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

void updateBatteryLED(int batteryLevel) {
    static unsigned long lastUpdate = 0;
    static int stableBatteryLevel = -1;
    unsigned long currentTime = millis();

    // Stabilize battery readings - only change if different for 2+ seconds
    static int lastRawLevel = -1;
    static unsigned long levelChangeTime = 0;

    if (batteryLevel != lastRawLevel) {
        lastRawLevel = batteryLevel;
        levelChangeTime = currentTime;
    } else if (currentTime - levelChangeTime > 2000 && stableBatteryLevel != batteryLevel) {
        stableBatteryLevel = batteryLevel;
    }

    if (stableBatteryLevel == -1) stableBatteryLevel = batteryLevel;

    // Update LED less frequently and with slower animations
    if (currentTime - lastUpdate < 1000) return; // Update every 1 second

    if (stableBatteryLevel >= 50) {
        // 50%+: Solid colors, no animation
        if (stableBatteryLevel >= 80) {
            myCodeCell.LED(0, 100, 0); // Dim green
        } else {
            myCodeCell.LED(100, 100, 0); // Dim yellow  
        }
    } else if (stableBatteryLevel >= 20) {
        myCodeCell.LED(100, 50, 0); // Dim orange
    } else {
        // Below 20%: Slow blink
        static bool blinkState = false;
        blinkState = !blinkState;
        myCodeCell.LED(blinkState ? 100 : 0, 0, 0);
    }

    lastUpdate = currentTime;
}

void setup() {
    Serial.begin(115200);
    Serial.println("CodeCell Computer BLE IMU Streamer with Auto-Reconnect and Battery LED");

    // Initialize all motion sensors
    myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION);

    // Set LED brightness to medium level
    myCodeCell.LED_SetBrightness(7);

    // Read initial battery level
    currentBatteryLevel = myCodeCell.BatteryLevelRead();
    lastBatteryRead = millis();

    // Initialize BLE
    BLEDevice::init("CodeCell");

    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

    // Add descriptor for notifications
    pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    Serial.println("Ready for computer GUI connection...");
}

void loop() {
    unsigned long currentTime = millis();

    // Read battery every 30 seconds (30000ms)
    if (currentTime - lastBatteryRead >= 30000) {
        currentBatteryLevel = myCodeCell.BatteryLevelRead();
        lastBatteryRead = currentTime;
        Serial.print("Battery updated: ");
        Serial.print(currentBatteryLevel);
        Serial.println("%");
    }

    if (myCodeCell.Run(dataRate)) {
        // Read all sensor data
        float roll, pitch, yaw;
        float ax, ay, az;
        float gx, gy, gz;

        myCodeCell.Motion_RotationRead(roll, pitch, yaw);
        myCodeCell.Motion_AccelerometerRead(ax, ay, az);
        myCodeCell.Motion_GyroRead(gx, gy, gz);

        // Send clean formatted data for computer GUI with cached battery level
        char message[120];
        sprintf(message, "R:%.3f,P:%.3f,Y:%.3f,AX:%.3f,AY:%.3f,AZ:%.3f,GX:%.3f,GY:%.3f,GZ:%.3f,B:%d",
                roll, pitch, yaw, ax, ay, az, gx, gy, gz, currentBatteryLevel);

        // Send via BLE if connected
        if (deviceConnected) {
            pCharacteristic->setValue(message);
            pCharacteristic->notify();
        }
    }

    // Always update LED with cached battery level
    updateBatteryLED(currentBatteryLevel);

    // Handle disconnection and restart advertising
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Give BLE stack time to get ready
        pServer->startAdvertising(); // Restart advertising
        Serial.println("Restarted advertising for reconnection");
        oldDeviceConnected = deviceConnected;
    }

    // Handle new connection
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("New connection established");
    }
}