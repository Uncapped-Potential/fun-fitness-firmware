/*
 * Phase 1: I2C Bus Scanner Test
 * 
 * Tests basic I2C communication on CodeCell hardware
 * Expected devices:
 * - 0x4A: BNO085 motion sensor
 * - 0x60: VCNL4040 light sensor (if present)
 * - Other power management devices
 * 
 * This helps isolate if I2C bus is fundamentally broken
 */

#include <CodeCell.h>
#include <Wire.h>

CodeCell myCodeCell;

// Track boot timing
unsigned long bootStartTime;
unsigned long i2cScanTime;
bool foundBNO085 = false;
bool foundVCNL = false;
int devicesFound = 0;

void setup() {
  bootStartTime = millis();
  
  // Initialize CodeCell with NO sensors to avoid conflicts
  myCodeCell.Init(MOTION_DISABLE);
  
  // Signal start of I2C test with LED pattern
  myCodeCell.LED(255, 255, 0); // Yellow = starting I2C scan
  delay(1000);
  
  // Initialize I2C with CodeCell's configuration
  Wire.begin(8, 9, 400000);  // SDA=GPIO8, SCL=GPIO9, 400kHz
  delay(500);
  
  i2cScanTime = millis();
  
  // Scan I2C bus for devices
  scanI2CBus();
  
  // Show results via LED
  displayResults();
}

void loop() {
  // Simple status blink
  if (myCodeCell.Run(1)) {  // 1Hz
    
    // Test power management (what controls green LED)
    uint8_t powerState = myCodeCell.PowerStateRead();
    
    // Blink pattern based on I2C scan results
    if (foundBNO085) {
      myCodeCell.LED(0, 255, 0); // Green = BNO085 found
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    } else {
      myCodeCell.LED(255, 0, 0); // Red = BNO085 missing
      delay(500);
      myCodeCell.LED(0, 0, 0);
      delay(500);
    }
    
    // Additional blinks for other devices
    for (int i = 0; i < devicesFound; i++) {
      myCodeCell.LED(0, 0, 255); // Blue blinks = device count
      delay(100);
      myCodeCell.LED(0, 0, 0);
      delay(100);
    }
    
    delay(2000);
  }
}

void scanI2CBus() {
  devicesFound = 0;
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      // Device found
      devicesFound++;
      
      // Check for known devices
      if (address == 0x4A) {
        foundBNO085 = true;
        // Signal BNO085 found
        myCodeCell.LED(0, 255, 0);
        delay(100);
        myCodeCell.LED(0, 0, 0);
        delay(100);
      }
      
      if (address == 0x60) {
        foundVCNL = true;
        // Signal VCNL found  
        myCodeCell.LED(0, 0, 255);
        delay(100);
        myCodeCell.LED(0, 0, 0);
        delay(100);
      }
      
      // Brief flash for any device found
      myCodeCell.LED(255, 255, 255);
      delay(50);
      myCodeCell.LED(0, 0, 0);
      delay(50);
    }
    
    delay(10); // Small delay between scans
  }
}

void displayResults() {
  unsigned long scanDuration = millis() - i2cScanTime;
  unsigned long totalBootTime = millis() - bootStartTime;
  
  // Display results via LED patterns
  if (devicesFound == 0) {
    // No devices found - critical failure
    for (int i = 0; i < 10; i++) {
      myCodeCell.LED(255, 0, 0); // Fast red blink
      delay(100);
      myCodeCell.LED(0, 0, 0);
      delay(100);
    }
  } else if (!foundBNO085) {
    // Devices found but no BNO085 - sensor issue
    for (int i = 0; i < 5; i++) {
      myCodeCell.LED(255, 165, 0); // Orange blink
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  } else {
    // BNO085 found - good!
    for (int i = 0; i < 3; i++) {
      myCodeCell.LED(0, 255, 0); // Green blink
      delay(300);
      myCodeCell.LED(0, 0, 0);
      delay(300);
    }
  }
  
  delay(1000);
  
  // Blink total boot time (seconds)
  for (int i = 0; i < (totalBootTime / 1000) && i < 10; i++) {
    myCodeCell.LED(255, 255, 255);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
  }
}