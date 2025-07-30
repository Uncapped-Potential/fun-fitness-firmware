/*
   * CodeCell Computer BLE IMU Streamer with Auto-Reconnect, Battery LED, and Sleep Mode
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

  int dataRate = 24; // Reduced from 50Hz to 24Hz for battery optimization

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
      Serial.println("CodeCell Computer BLE IMU Streamer with Auto-Reconnect and Battery LED");

      // Initialize all motion sensors including magnetometer
      myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER);

      // Initialize BLE
      BLEDevice::init("CodeCell");
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
          // Read all sensor data
          float roll, pitch, yaw;
          float ax, ay, az;
          float gx, gy, gz;

          myCodeCell.Motion_RotationRead(roll, pitch, yaw);
          myCodeCell.Motion_AccelerometerRead(ax, ay, az);
          myCodeCell.Motion_GyroRead(gx, gy, gz);

          // Read battery fresh every cycle (50Hz) - minimal power impact
          int batteryLevel = myCodeCell.BatteryLevelRead();

          // Send clean formatted data for computer GUI with fresh battery level
          char message[120];
          sprintf(message, "R:%.3f,P:%.3f,Y:%.3f,AX:%.3f,AY:%.3f,AZ:%.3f,GX:%.3f,GY:%.3f,GZ:%.3f,B:%d",
                  roll, pitch, yaw, ax, ay, az, gx, gy, gz, batteryLevel);

          // Send via BLE if connected
          if (deviceConnected) {
              pCharacteristic->setValue(message);
              pCharacteristic->notify();
          }
      }

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