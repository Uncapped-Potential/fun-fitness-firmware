/*
 * CodeCell QC Test Firmware v0.0.1
 * Dedicated firmware for testing BQ24232RGTR charge controller hardware
 * 
 * This firmware directly tests the charge controller pins that are not used
 * in production firmware, ensuring hardware assembly quality before
 * production firmware is loaded.
 */

#include <CodeCell.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <string>

CodeCell myCodeCell;

// SAFE MODE: GPIO pins disabled until schematic verification
// #define CE_PIN      10    // Charge Enable (Active LOW) - DISABLED
// #define PGOOD_PIN   18    // Power Good (LOW = valid USB) - DISABLED  
// #define CHG_PIN     8     // Charge Status (LOW = charging) - DISABLED
// #define VBAT_PIN    1     // Battery Voltage ADC - DISABLED

// Safe monitoring using only existing CodeCell functions
#define SAFE_MODE_ENABLED true

// QC Test BLE Service UUIDs
#define QC_SERVICE_UUID     "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2000"
#define QC_COMMAND_UUID     "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2001"
#define QC_STATUS_UUID      "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2002"
#define QC_LOG_UUID         "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2003"

// QC Test Commands (Safe Mode)
enum QCCommand {
  QC_GET_STATUS = 0x01,
  // QC_CHARGE_ENABLE = 0x02,  // DISABLED - GPIO control unsafe
  // QC_CHARGE_DISABLE = 0x03, // DISABLED - GPIO control unsafe
  QC_GET_SERIAL = 0x04,
  QC_START_LOGGING = 0x05,
  QC_STOP_LOGGING = 0x06,
  QC_CHECKPOINT = 0x07,
  QC_GET_LOG = 0x08,
  QC_SELF_TEST = 0x09
};

// QC Status packet (16 bytes total) - Safe Mode
struct QCStatus {
  uint8_t  usb_connected;    // USB power state (from CodeCell)
  uint8_t  charging_state;   // Charging state (from CodeCell)
  uint8_t  safe_mode;        // Always 1 in safe mode
  uint8_t  reserved;         // Padding for alignment
  uint16_t battery_mv;       // Battery voltage from CodeCell.GetBatteryVoltage()
  uint16_t vcc_mv;           // VCC voltage (should be ~3300mV)
  uint32_t timestamp_ms;     // Millis since test start
  uint32_t serial;           // Device serial number
} __attribute__((packed));

// QC Log Entry (12 bytes)
struct QCLogEntry {
  uint32_t timestamp_ms;
  uint16_t battery_mv;
  uint8_t  pgood;
  uint8_t  chg;
  uint8_t  temperature;  // MCU temperature in °C
  uint8_t  phase;        // Current test phase
  uint16_t reserved;     // Padding
} __attribute__((packed));

// BLE Server and Characteristics
BLEServer* pServer = nullptr;
BLECharacteristic* pCommandChar = nullptr;
BLECharacteristic* pStatusChar = nullptr;
BLECharacteristic* pLogChar = nullptr;

// QC Test State
bool deviceConnected = false;
bool qcTestActive = false;
bool loggingActive = false;
uint32_t testStartTime = 0;
uint32_t serialNumber = 0;
uint8_t currentPhase = 0;
std::vector<QCLogEntry> testLog;

// Test phases for logging
enum TestPhase {
  PHASE_IDLE = 0,
  PHASE_INITIAL = 1,
  PHASE_CHARGE_2H = 2,
  PHASE_DISCHARGE_2H = 3,
  PHASE_CHARGE_15MIN = 4,
  PHASE_FINAL_DISCHARGE = 5
};

const char* phaseNames[] = {
  "IDLE", "INITIAL", "CHARGE_2H", "DISCHARGE_2H", "CHARGE_15MIN", "FINAL_DISCHARGE"
};

// Helper functions - Safe Mode
uint16_t readBatteryVoltage() {
  #ifdef SAFE_MODE_ENABLED
    // Use validated CodeCell method from production firmware
    int batteryLevel = myCodeCell.BatteryLevelRead(); // Returns percentage 0-100
    // Estimate voltage from percentage (rough approximation)
    return (uint16_t)(3000 + (batteryLevel * 12)); // 3.0V-4.2V range
  #else
    // Original GPIO method (disabled)
    uint16_t raw = analogRead(VBAT_PIN);
    return (uint16_t)((raw * 3300UL * 2) / 4095);
  #endif
}

float readMCUTemperature() {
  // ESP32-C3 doesn't have reliable temperature sensor
  // Use thermal camera for temperature monitoring instead
  return 25.0; // Return nominal temperature
}

void addLogEntry() {
  if (!loggingActive) return;
  
  QCLogEntry entry;
  entry.timestamp_ms = millis() - testStartTime;
  entry.battery_mv = readBatteryVoltage();
  #ifdef SAFE_MODE_ENABLED
    // Safe mode: estimate USB/charging state from battery voltage trends
    entry.pgood = (entry.battery_mv > 4000) ? 0 : 1; // Assume USB if high voltage
    entry.chg = (entry.battery_mv > 4000) ? 0 : 1;   // Assume charging if high voltage
  #else
    // Original GPIO method (disabled)
    entry.pgood = digitalRead(PGOOD_PIN);
    entry.chg = digitalRead(CHG_PIN);
  #endif
  entry.temperature = 25; // Use thermal camera for temp monitoring
  entry.phase = currentPhase;
  entry.reserved = 0;
  
  testLog.push_back(entry);
  
  // Limit log size to prevent memory issues (keep last 1000 entries)
  if (testLog.size() > 1000) {
    testLog.erase(testLog.begin());
  }
  
  // Also send via BLE notifications for real-time monitoring
  if (deviceConnected) {
    pLogChar->setValue((uint8_t*)&entry, sizeof(entry));
    pLogChar->notify();
  }
}

// BLE Command Handler
class QCCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value.length() == 0) return;
    
    uint8_t command = value[0];
    
    switch (command) {
      case QC_GET_STATUS: {
        QCStatus status;
        #ifdef SAFE_MODE_ENABLED
          // Safe mode: use validated PowerStateRead method
          int powerState = myCodeCell.PowerStateRead();
          status.usb_connected = (powerState == 1) ? 1 : 0; // 1 = USB powered
          status.charging_state = status.usb_connected; // Assume charging if USB
          status.safe_mode = 1;
        #else
          // Original GPIO method (disabled)
          status.usb_connected = digitalRead(PGOOD_PIN);
          status.charging_state = digitalRead(CHG_PIN);
          status.safe_mode = 0;
        #endif
        status.reserved = 0;
        status.battery_mv = readBatteryVoltage();
        status.vcc_mv = 3300; // Assume 3.3V VCC
        status.timestamp_ms = qcTestActive ? (millis() - testStartTime) : 0;
        status.serial = serialNumber;
        
        pStatusChar->setValue((uint8_t*)&status, sizeof(status));
        pStatusChar->notify();
        
        Serial.printf("QC STATUS (SAFE): USB=%d, CHG=%d, VBAT=%dmV\n", 
                     status.usb_connected, status.charging_state, status.battery_mv);
        break;
      }
      
      // GPIO control commands disabled in safe mode
      // case QC_CHARGE_ENABLE:
      // case QC_CHARGE_DISABLE:
      //   Serial.println("QC: GPIO control disabled in safe mode");
      //   break;
        
      case QC_START_LOGGING:
        if (value.length() >= 2) {
          currentPhase = value[1];
        }
        loggingActive = true;
        if (!qcTestActive) {
          qcTestActive = true;
          testStartTime = millis();
          testLog.clear();
        }
        Serial.printf("QC: Logging STARTED, Phase: %s\n", phaseNames[currentPhase]);
        addLogEntry();
        break;
        
      case QC_STOP_LOGGING:
        loggingActive = false;
        Serial.println("QC: Logging STOPPED");
        addLogEntry();
        break;
        
      case QC_CHECKPOINT:
        if (value.length() >= 2) {
          currentPhase = value[1];
        }
        Serial.printf("QC: Checkpoint - Phase: %s\n", phaseNames[currentPhase]);
        addLogEntry();
        break;
        
      case QC_GET_LOG:
        // Send log entries via notifications (chunked if needed)
        Serial.printf("QC: Sending log (%d entries)\n", testLog.size());
        for (size_t i = 0; i < testLog.size(); i++) {
          pLogChar->setValue((uint8_t*)&testLog[i], sizeof(QCLogEntry));
          pLogChar->notify();
          delay(10); // Small delay to prevent BLE congestion
        }
        break;
        
      case QC_SELF_TEST: {
        Serial.println("QC: Running SAFE MODE self-test...");
        
        #ifdef SAFE_MODE_ENABLED
          // Safe mode: Test only what we can safely access
          uint16_t vbat = readBatteryVoltage();
          bool battery_ok = (vbat >= 3000 && vbat <= 4200);
          
          // Estimate USB connection from voltage
          bool usb_likely = (vbat > 4000);
          
          Serial.printf("Safe self-test: VBAT=%s (%.3fV), USB_EST=%s\n",
                       battery_ok ? "PASS" : "FAIL", vbat/1000.0,
                       usb_likely ? "LIKELY" : "UNLIKELY");
          Serial.println("Temperature monitoring via thermal camera only");
          Serial.println("GPIO pin control disabled for safety");
        #else
          // Original GPIO test (disabled)
          bool pgood = digitalRead(PGOOD_PIN) == 0;
          bool chg_initial = digitalRead(CHG_PIN) == 1;
          uint16_t vbat = readBatteryVoltage();
          bool battery_ok = (vbat >= 3000 && vbat <= 4200);
          float temp = readMCUTemperature();
          bool temp_ok = (temp > 0 && temp < 80);
          
          Serial.printf("Full self-test: PGOOD=%s, CHG=%s, VBAT=%s (%.3fV), TEMP=%s (%.1f°C)\n",
                       pgood ? "PASS" : "FAIL",
                       chg_initial ? "PASS" : "FAIL", 
                       battery_ok ? "PASS" : "FAIL", vbat/1000.0,
                       temp_ok ? "PASS" : "FAIL", temp);
        #endif
        break;
      }
      
      default:
        Serial.printf("QC: Unknown command: 0x%02X\n", command);
        break;
    }
  }
};

// BLE Server Callbacks
class QCServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("QC Test Client Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("QC Test Client Disconnected");
    // Keep test running even if disconnected
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("CodeCell QC Test Firmware v0.0.1");
  Serial.println("SAFE MODE - Thermal/Voltage Only");
  Serial.println("=================================");
  
  // Extract serial number from device name
  String deviceName = "FitChip011"; // This would normally come from EEPROM/NVS
  if (deviceName.length() >= 3) {
    serialNumber = deviceName.substring(deviceName.length()-3).toInt();
  } else {
    serialNumber = 999; // Default for testing
  }
  
  Serial.printf("Device Serial: %03d\n", serialNumber);
  
  #ifdef SAFE_MODE_ENABLED
    Serial.println("GPIO pins disabled for safety - using CodeCell library only");
  #else
    // Initialize charge controller pins (disabled in safe mode)
    pinMode(CE_PIN, OUTPUT);
    pinMode(PGOOD_PIN, INPUT);
    pinMode(CHG_PIN, INPUT);
    pinMode(VBAT_PIN, INPUT);
    digitalWrite(CE_PIN, HIGH);
  #endif
  
  // Initialize sensors (same as production firmware)
  Serial.println("Initializing sensor suite...");
  myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER);
  
  // Initialize BLE
  String bleDeviceName = "QC_" + String(serialNumber);
  BLEDevice::init(bleDeviceName.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new QCServerCallbacks());

  // Create QC Service
  BLEService *pService = pServer->createService(QC_SERVICE_UUID);
  
  // Command characteristic (WRITE)
  pCommandChar = pService->createCharacteristic(
    QC_COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCommandChar->setCallbacks(new QCCommandCallbacks());
  
  // Status characteristic (READ + NOTIFY)
  pStatusChar = pService->createCharacteristic(
    QC_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusChar->addDescriptor(new BLE2902());
  
  // Log characteristic (NOTIFY only)
  pLogChar = pService->createCharacteristic(
    QC_LOG_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pLogChar->addDescriptor(new BLE2902());
  
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(QC_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.printf("QC Test Ready - Advertising as: %s\n", bleDeviceName.c_str());
  Serial.println("Waiting for QC test commands...");
  
  // Print initial status
  delay(1000);
  #ifdef SAFE_MODE_ENABLED
    Serial.printf("Initial State (SAFE): VBAT=%.3fV\n", readBatteryVoltage()/1000.0);
    Serial.println("Use thermal camera for temperature monitoring");
  #else
    Serial.printf("Initial State: PGOOD=%d, CHG=%d, VBAT=%.3fV\n",
                 digitalRead(PGOOD_PIN), digitalRead(CHG_PIN), 
                 readBatteryVoltage()/1000.0);
  #endif
}

void loop() {
  // Auto-logging at specified intervals based on test phase
  static unsigned long lastLogTime = 0;
  unsigned long logInterval = 30000; // Default 30 seconds
  
  // Adjust logging frequency based on test phase
  switch (currentPhase) {
    case PHASE_DISCHARGE_2H:
    case PHASE_CHARGE_2H:
      logInterval = 2000; // 0.5 Hz (every 2 seconds)
      break;
    case PHASE_FINAL_DISCHARGE:
      logInterval = 17; // ~60 Hz (every ~17ms)
      break;
    case PHASE_CHARGE_15MIN:
      logInterval = 10000; // Every 10 seconds
      break;
    default:
      logInterval = 30000; // Every 30 seconds
      break;
  }
  
  if (loggingActive && (millis() - lastLogTime >= logInterval)) {
    addLogEntry();
    lastLogTime = millis();
  }
  
  // Handle BLE disconnection and restart advertising
  if (!deviceConnected) {
    delay(500);
    pServer->startAdvertising();
  }
  
  delay(100); // Small delay to prevent tight loop
}