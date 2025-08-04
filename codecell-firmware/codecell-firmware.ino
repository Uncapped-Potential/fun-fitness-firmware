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

          // Read battery fresh every cycle (24Hz) - minimal power impact
          int batteryLevel = myCodeCell.BatteryLevelRead();

          /*
           * BINARY PROTOCOL DOCUMENTATION
           * =============================
           * 
           * Total packet size: 19 bytes (vs ~85 bytes text = 77% reduction)
           * 
           * Byte Layout:
           * [0-1]   Roll (int16_t)      - Range: ±327°,     Scale: 0.01°,   Precision: 0.01°
           * [2-3]   Pitch (int16_t)     - Range: ±327°,     Scale: 0.01°,   Precision: 0.01°
           * [4-5]   Yaw (int16_t)       - Range: ±327°,     Scale: 0.01°,   Precision: 0.01°
           * [6-7]   Accel X (int16_t)   - Range: ±65.5g,    Scale: 0.002g,  Precision: 0.002g
           * [8-9]   Accel Y (int16_t)   - Range: ±65.5g,    Scale: 0.002g,  Precision: 0.002g
           * [10-11] Accel Z (int16_t)   - Range: ±65.5g,    Scale: 0.002g,  Precision: 0.002g
           * [12-13] Gyro X (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s,  Precision: 0.1°/s
           * [14-15] Gyro Y (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s,  Precision: 0.1°/s
           * [16-17] Gyro Z (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s,  Precision: 0.1°/s
           * [18]    Battery (uint8_t)   - Range: 0-255,     Direct mapping
           * 
           * Encoding Process:
           * 1. Scale float to integer: floatValue * scaleFactor
           * 2. Cast to int16_t: (int16_t)(scaledValue)
           * 3. Pack into bytes (little-endian): lowByte = value & 0xFF, highByte = (value >> 8) & 0xFF
           * 
           * Decoding Process (for web app):
           * 1. Unpack bytes: value = lowByte + (highByte << 8)
           * 2. Convert to signed if needed: if (value > 32767) value -= 65536
           * 3. Unscale to float: floatValue = value / scaleFactor
           * 
           * Example: Roll = 45.123°
           * 1. Scale: 45.123 * 100 = 4512.3 → 4512 (int16_t)
           * 2. Pack: binaryData[0] = 4512 & 0xFF = 160, binaryData[1] = (4512 >> 8) & 0xFF = 17
           * 3. Decode: value = 160 + (17 << 8) = 4512, roll = 4512 / 100.0 = 45.12°
           * 
           * Scaling Factors Chosen For:
           * - Rotation: Covers full ±180° with 0.01° precision (excellent for orientation)
           * - Acceleration: Covers vigorous motion ±65.5g with 0.002g precision (handles gaming)
           * - Gyroscope: Covers rapid rotation ±3276°/s with 0.1°/s precision (smooth tracking)
           */
          
          uint8_t binaryData[19];
          
          // Convert floats to scaled int16_t values (all ranges verified to fit in int16_t)
          int16_t rollInt = (int16_t)(roll * 100);        // 0.01° precision, ±327° range
          int16_t pitchInt = (int16_t)(pitch * 100);      // 0.01° precision, ±327° range
          int16_t yawInt = (int16_t)(yaw * 100);          // 0.01° precision, ±327° range
          int16_t axInt = (int16_t)(ax * 500);            // 0.002g precision, ±65.5g range (increased for vigorous motion)
          int16_t ayInt = (int16_t)(ay * 500);            // 0.002g precision, ±65.5g range (increased for vigorous motion)
          int16_t azInt = (int16_t)(az * 500);            // 0.002g precision, ±65.5g range (increased for vigorous motion)
          int16_t gxInt = (int16_t)(gx * 10);             // 0.1°/s precision, ±3276°/s range
          int16_t gyInt = (int16_t)(gy * 10);             // 0.1°/s precision, ±3276°/s range
          int16_t gzInt = (int16_t)(gz * 10);             // 0.1°/s precision, ±3276°/s range
          uint8_t batteryByte = (uint8_t)batteryLevel;    // Direct 0-255 mapping
          
          // Pack into binary array using little-endian byte order (ESP32 native, web compatible)
          binaryData[0] = rollInt & 0xFF;           // Roll low byte
          binaryData[1] = (rollInt >> 8) & 0xFF;    // Roll high byte
          binaryData[2] = pitchInt & 0xFF;          // Pitch low byte
          binaryData[3] = (pitchInt >> 8) & 0xFF;   // Pitch high byte
          binaryData[4] = yawInt & 0xFF;            // Yaw low byte
          binaryData[5] = (yawInt >> 8) & 0xFF;     // Yaw high byte
          binaryData[6] = axInt & 0xFF;             // Accel X low byte
          binaryData[7] = (axInt >> 8) & 0xFF;      // Accel X high byte
          binaryData[8] = ayInt & 0xFF;             // Accel Y low byte
          binaryData[9] = (ayInt >> 8) & 0xFF;      // Accel Y high byte
          binaryData[10] = azInt & 0xFF;            // Accel Z low byte
          binaryData[11] = (azInt >> 8) & 0xFF;     // Accel Z high byte
          binaryData[12] = gxInt & 0xFF;            // Gyro X low byte
          binaryData[13] = (gxInt >> 8) & 0xFF;     // Gyro X high byte
          binaryData[14] = gyInt & 0xFF;            // Gyro Y low byte
          binaryData[15] = (gyInt >> 8) & 0xFF;     // Gyro Y high byte
          binaryData[16] = gzInt & 0xFF;            // Gyro Z low byte
          binaryData[17] = (gzInt >> 8) & 0xFF;     // Gyro Z high byte
          binaryData[18] = batteryByte;             // Battery (single byte)

          // Send compact binary data via BLE (19 bytes vs ~85 bytes = 77% size reduction)
          if (deviceConnected) {
              pCharacteristic->setValue(binaryData, 19);
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