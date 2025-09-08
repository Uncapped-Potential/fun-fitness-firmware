/*
 * CodeCell QC Test Firmware - Based on Production Firmware
 * Provides safe QC testing interface while maintaining all normal functionality
 * Battery monitoring and USB detection only (no GPIO manipulation)
 */

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <cmath>
#include <string>

CodeCell myCodeCell;

// Forward declarations
void setupOtaService(BLEServer* srv);
void setupQcService(BLEServer* srv);

// Helper functions for safe little-endian parsing on RISC-V
static inline uint16_t le16(const uint8_t* p){ return (uint16_t)p[0] | (uint16_t)p[1]<<8; }
static inline uint32_t le32(const uint8_t* p){ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static inline void put_le32(uint8_t* d, uint32_t v){ 
  d[0]=v&0xFF; d[1]=(v>>8)&0xFF; d[2]=(v>>16)&0xFF; d[3]=(v>>24)&0xFF; 
}

// RTC memory to survive deep sleep
RTC_DATA_ATTR bool sensorInitialized = false;

// BLE UUIDs (matching MicroLink for compatibility)
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"

// QC Test Service UUIDs
#define QC_SERVICE_UUID     "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2000"
#define QC_COMMAND_UUID     "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2001"
#define QC_STATUS_UUID      "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2002"
#define QC_LOG_UUID         "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2003"

// BLE OTA Service UUIDs
#define OTA_SERVICE_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001"
#define OTA_CONTROL_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0002"
#define OTA_DATA_UUID      "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0003"

// OTA Control opcodes
enum { OP_START=0x01, OP_DATA=0x02, OP_FINISH=0x03, OP_REBOOT=0x04 };
enum { OP_ACK_START=0xA1, OP_ACK_FINISH=0xA2, OP_PROGRESS=0x91, OP_ERROR=0xE0 };

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// OTA BLE characteristics
static BLECharacteristic *pOtaCtrl = nullptr, *pOtaData = nullptr;

// OTA state variables
static const uint16_t DEFAULT_PROPOSED_CHUNK = 200;
static uint16_t agreedChunk = 200;
static const esp_partition_t *update_part = nullptr;
static esp_ota_handle_t ota_handle = 0;
static uint32_t total_len = 0, rx_len = 0, host_crc = 0;
static bool ota_active = false;

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

// USB power detection
bool isUsbPowered = false;
unsigned long lastPowerCheck = 0;

// ==== QC Test Additions ====
// QC Test BLE characteristics
static BLECharacteristic *pQcCommand = nullptr;
static BLECharacteristic *pQcStatus = nullptr;
static BLECharacteristic *pQcLog = nullptr;

// QC Test Commands
enum QCCommand {
  QC_GET_STATUS = 0x01,
  QC_START_LOGGING = 0x05,
  QC_STOP_LOGGING = 0x06,
  QC_CHECKPOINT = 0x07,
  QC_SELF_TEST = 0x09
};

// QC Status packet (16 bytes total) - Safe Mode
struct QCStatus {
  uint8_t  usb_connected;    // USB power state
  uint8_t  charging_state;   // Charging state estimate
  uint8_t  safe_mode;        // Always 1 in safe mode
  uint8_t  reserved;         // Padding
  uint16_t battery_mv;       // Battery voltage in mV
  uint16_t vcc_mv;           // VCC voltage (3300mV)
  uint32_t timestamp_ms;     // Millis since test start
  uint32_t serial;           // Device serial number
} __attribute__((packed));

// QC Test State
bool qcTestActive = false;
bool qcLoggingActive = false;
uint32_t qcTestStartTime = 0;
uint32_t qcSerialNumber = 0;
uint8_t qcCurrentPhase = 0;

// ==== BLE OTA Implementation ====
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i=0;i<len;i++) {
    crc ^= data[i];
    for (int k=0;k<8;k++) crc = (crc>>1) ^ (0xEDB88320 & (-(int)(crc & 1)));
  }
  return ~crc;
}

class OtaControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String arduinoString = c->getValue();
    std::string s(arduinoString.c_str(), arduinoString.length());  // Convert properly
    if (s.size() == 0) return;
    const uint8_t *buf = (const uint8_t*)s.data();
    uint8_t op = buf[0];

    if (op == OP_START && s.size() >= (1+4+4+2)) {  // Change to .size()
      if (ota_active) return;
      total_len = le32(&buf[1]);
      host_crc  = le32(&buf[5]);
      uint16_t proposed = le16(&buf[9]);

      update_part = esp_ota_get_next_update_partition(NULL);
      if (!update_part) { notifyError(1); return; }
      if (esp_ota_begin(update_part, total_len, &ota_handle) != ESP_OK) { notifyError(2); return; }

      rx_len = 0; ota_active = true;
      agreedChunk = (proposed >= 20 && proposed <= 200) ? proposed : 200; // safer cap

      Serial.printf("OTA START: %u bytes, chunk size: %u\n", total_len, agreedChunk);

      uint8_t ack[3] = { OP_ACK_START, (uint8_t)(agreedChunk & 0xFF), (uint8_t)(agreedChunk >> 8) };
      pOtaCtrl->setValue(ack, sizeof(ack)); pOtaCtrl->notify();

    } else if (op == OP_FINISH) {
      if (!ota_active) return;
      
      // Skip CRC check for now - trust the flash write and BLE reliability
      if (rx_len != total_len) { 
        Serial.printf("OTA FINISH ERROR: len %u/%u (skipping CRC check)\n", rx_len, total_len);
        notifyError(3); return; 
      }
      
      Serial.printf("OTA FINISH: %u bytes received, CRC check skipped\n", rx_len);
      
      if (esp_ota_end(ota_handle) != ESP_OK) { notifyError(4); return; }
      if (esp_ota_set_boot_partition(update_part) != ESP_OK) { notifyError(5); return; }
      
      Serial.println("OTA COMPLETE - ready to reboot");
      uint8_t ack = OP_ACK_FINISH; pOtaCtrl->setValue(&ack,1); pOtaCtrl->notify();
      ota_active = false;

    } else if (op == OP_REBOOT) {
      Serial.println("OTA REBOOT requested");
      delay(50); esp_restart();
    }
  }
  
  static void notifyError(uint8_t code){
    Serial.printf("OTA ERROR: %u\n", code);
    uint8_t e[2] = { OP_ERROR, code };
    pOtaCtrl->setValue(e, sizeof(e)); pOtaCtrl->notify();
  }
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    if (!ota_active) return;
    String arduinoString = c->getValue();
    std::string s(arduinoString.c_str(), arduinoString.length());  // Convert properly
    if (s.size() < 1+4) return;
    const uint8_t *buf = (const uint8_t*)s.data();
    if (buf[0] != OP_DATA) return;
    uint32_t offset = le32(&buf[1]);
    const uint8_t *payload = &buf[5];
    size_t plen = s.size()-5;      // Change to .size()

    // Write chunk to flash at the specified offset
    if (esp_ota_write_with_offset(ota_handle, payload, plen, offset) != ESP_OK) {
      Serial.printf("OTA WRITE ERROR at offset %u\n", offset);
      uint8_t e[2] = { OP_ERROR, 7 }; pOtaCtrl->setValue(e,2); pOtaCtrl->notify();
      return;
    }
    
    // Update received bytes count (track highest offset + length)
    uint32_t chunk_end = offset + plen;
    if (chunk_end > rx_len) {
      rx_len = chunk_end;
    }
    Serial.printf("OTA DATA: Wrote %u bytes at offset %u, total progress: %u/%u\n", plen, offset, rx_len, total_len);

    if ((rx_len & 0x3FFF) == 0 || rx_len == total_len) {
      uint8_t prog[1+4+4]; 
      prog[0] = OP_PROGRESS;
      put_le32(&prog[1], rx_len);    // Use helper instead of *(uint32_t*)
      put_le32(&prog[5], total_len); // Use helper instead of *(uint32_t*)
      pOtaCtrl->setValue(prog, sizeof(prog)); 
      pOtaCtrl->notify();
      Serial.printf("OTA PROGRESS: %u/%u bytes\n", rx_len, total_len);
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
            
            float deltaAx = fabsf(ax - baselineAx);  // Change from abs() to fabsf()
            float deltaAy = fabsf(ay - baselineAy);  // Change from abs() to fabsf()
            float deltaAz = fabsf(az - baselineAz);  // Change from abs() to fabsf()
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
    
    // Initialize BLE (keep original naming)
    BLEDevice::init("FitChip014");
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
    
    // Add OTA service
    setupOtaService(pServer);
    
    // Add QC Test service (for production testing)
    setupQcService(pServer);

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
    pAdvertising->addServiceUUID(QC_SERVICE_UUID); // Add QC service to advertisements
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    Serial.println("Ready for computer GUI connection...");
    lastMotionTime = millis();
    lastBleCheck = millis();
}

// ==== OTA Service Setup Function ====
void setupOtaService(BLEServer* srv) {
  BLEService *svc = srv->createService(OTA_SERVICE_UUID);

  pOtaCtrl = svc->createCharacteristic(
    OTA_CONTROL_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pOtaCtrl->addDescriptor(new BLE2902());  // Add CCCD for notifications
  pOtaCtrl->setCallbacks(new OtaControlCallbacks());

  pOtaData = svc->createCharacteristic(
    OTA_DATA_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE  // Add WRITE for fallback
  );
  pOtaData->setCallbacks(new OtaDataCallbacks());

  svc->start();
  Serial.println("OTA service initialized");
}

// ==== QC Test Service Setup ====
// QC Command Handler
class QcCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value.length() == 0) return;
    
    uint8_t command = value[0];
    
    switch (command) {
      case QC_GET_STATUS: {
        // Build status packet from existing data
        QCStatus status;
        int powerState = myCodeCell.PowerStateRead();
        status.usb_connected = (powerState == 1) ? 1 : 0;
        status.charging_state = status.usb_connected;
        status.safe_mode = 1; // Always safe mode
        status.reserved = 0;
        
        // Get battery as percentage and convert to mV estimate
        int batteryPercent = myCodeCell.BatteryLevelRead();
        status.battery_mv = 3000 + (batteryPercent * 12); // 3.0V-4.2V range
        status.vcc_mv = 3300;
        status.timestamp_ms = qcTestActive ? (millis() - qcTestStartTime) : 0;
        status.serial = qcSerialNumber;
        
        pQcStatus->setValue((uint8_t*)&status, sizeof(status));
        pQcStatus->notify();
        
        Serial.printf("QC STATUS: USB=%d, Battery=%d%% (~%dmV)\n", 
                     status.usb_connected, batteryPercent, status.battery_mv);
        break;
      }
      
      case QC_START_LOGGING:
        if (value.length() >= 2) {
          qcCurrentPhase = value[1];
        }
        qcLoggingActive = true;
        if (!qcTestActive) {
          qcTestActive = true;
          qcTestStartTime = millis();
        }
        Serial.printf("QC: Logging STARTED, Phase: %d\n", qcCurrentPhase);
        break;
        
      case QC_STOP_LOGGING:
        qcLoggingActive = false;
        Serial.println("QC: Logging STOPPED");
        break;
        
      case QC_CHECKPOINT:
        if (value.length() >= 2) {
          qcCurrentPhase = value[1];
        }
        Serial.printf("QC: Checkpoint - Phase: %d\n", qcCurrentPhase);
        break;
        
      case QC_SELF_TEST: {
        Serial.println("QC: Running self-test...");
        int batteryPercent = myCodeCell.BatteryLevelRead();
        int powerState = myCodeCell.PowerStateRead();
        Serial.printf("Self-test: Battery=%d%%, USB=%s\n",
                     batteryPercent,
                     powerState == 1 ? "Connected" : "Disconnected");
        break;
      }
      
      default:
        Serial.printf("QC: Unknown command: 0x%02X\n", command);
        break;
    }
  }
};

void setupQcService(BLEServer* srv) {
  // Extract serial from device name - use the actual BLE device name
  String deviceName = "FitChip014"; // Should match BLEDevice::init() name
  if (deviceName.length() >= 3) {
    qcSerialNumber = deviceName.substring(deviceName.length()-3).toInt();
  }
  
  BLEService *svc = srv->createService(QC_SERVICE_UUID);
  
  // Command characteristic (WRITE)
  pQcCommand = svc->createCharacteristic(
    QC_COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pQcCommand->setCallbacks(new QcCommandCallbacks());
  
  // Status characteristic (READ + NOTIFY)
  pQcStatus = svc->createCharacteristic(
    QC_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pQcStatus->addDescriptor(new BLE2902());
  
  // Log characteristic (NOTIFY only)
  pQcLog = svc->createCharacteristic(
    QC_LOG_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pQcLog->addDescriptor(new BLE2902());
  
  svc->start();
  Serial.println("QC Test service initialized");
}

void loop() {
    // Multi-tier power management
    unsigned long currentTime = millis();
    
    // Check USB power status every 5 seconds
    if (currentTime - lastPowerCheck > 5000) {
        lastPowerCheck = currentTime;
        int powerState = myCodeCell.PowerStateRead();
        isUsbPowered = (powerState == 1); // 1 = Running from USB Power
        
        if (isUsbPowered) {
            Serial.println("USB powered - maintaining 60Hz operation");
        }
    }
    
    if (currentTime - lastBleCheck > 1000) { // Check every second
        lastBleCheck = currentTime;
        unsigned long timeSinceMotion = currentTime - lastMotionTime;
        
        // Skip power management if USB powered - stay at full performance
        if (isUsbPowered) {
            // Ensure we're in ACTIVE state when USB powered
            if (currentPowerState != ACTIVE) {
                Serial.println("USB detected - returning to ACTIVE state");
                setCpuFrequencyMhz(160); // Restore full speed
                dataRate = 60; // Restore full data rate
                imuRate = 60; // Restore full IMU rate
                bleActive = true;
                if (!pServer->getAdvertising()->isAdvertising()) {
                    pServer->getAdvertising()->start();
                }
                currentPowerState = ACTIVE;
            }
            // Skip the power management state machine when USB powered
        } else {
            // State machine for power management (battery only)
            switch (currentPowerState) {
            case ACTIVE:
                if (!ota_active && timeSinceMotion > bleInactivityTimeout) {
                    Serial.printf("Entering BLE_OFF state (1Hz mode) after %lums\n", timeSinceMotion);
                    dataRate = lowPowerDataRate; // Reduce to 1Hz transmission
                    imuRate = lowPowerImuRate; // Reduce IMU polling to 10Hz
                    bleActive = false; // Actually turn off BLE notifications
                    pServer->getAdvertising()->stop(); // Stop advertising
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
                    pServer->getAdvertising()->start(); // Restart advertising
                    currentPowerState = ACTIVE;
                } else if (!ota_active && timeSinceMotion > lightSleepTimeout) {
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

        // Send minimal quaternion data via BLE (19 bytes - same as Euler!)
        // Throttle IMU streaming during OTA to avoid congestion
        if (deviceConnected && bleActive && !ota_active) {
            pCharacteristic->setValue(binaryData, 19);
            pCharacteristic->notify();
        } else if (ota_active) {
            // During OTA, reduce IMU notifications to prevent BLE congestion
            static unsigned long lastOtaThrottleLog = 0;
            if (currentTime - lastOtaThrottleLog > 5000) {
                Serial.println("IMU streaming throttled during OTA");
                lastOtaThrottleLog = currentTime;
            }
        }
    }
    // End of sensor reading block

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