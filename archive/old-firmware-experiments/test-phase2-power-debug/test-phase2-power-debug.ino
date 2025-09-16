/*
 * Phase 2: Power Management Debug Test with Serial Output
 * 
 * Tests power management with detailed serial logging
 * Shows exact values returned by power functions
 */

#include <CodeCell.h>

CodeCell myCodeCell;

// Timing tracking
unsigned long bootStartTime;
unsigned long powerInitTime;
bool powerSystemWorking = false;
uint8_t lastPowerState = 255;
uint16_t lastBatteryVoltage = 0;
uint8_t lastBatteryLevel = 0;

void setup() {
  // Initialize Serial FIRST (like main firmware)
  Serial.begin(115200);
  delay(1000);
  
  bootStartTime = millis();
  
  Serial.println("=================================");
  Serial.println("Phase 2: Power Management Debug");
  Serial.println("=================================");
  Serial.printf("Boot start time: %lu ms\n", bootStartTime);
  
  // Initialize CodeCell with NO sensors
  Serial.println("Initializing CodeCell (no sensors)...");
  myCodeCell.Init(MOTION_DISABLE);
  
  powerInitTime = millis();
  Serial.printf("CodeCell init completed at: %lu ms\n", powerInitTime);
  
  // Signal start of power test
  myCodeCell.LED(0, 255, 255); // Cyan = power test starting
  delay(1000);
  
  // Test power management functions
  testPowerManagementDetailed();
  
  // Show results
  displayPowerResults();
}

void loop() {
  if (myCodeCell.Run(2)) {  // 2Hz rate
    
    // Continuously monitor power system
    monitorPowerSystemDetailed();
    
    // Display current power state via LED
    displayPowerState();
    
    delay(500);
  }
}

void testPowerManagementDetailed() {
  Serial.println("\n--- Power Management Stabilization Test ---");
  Serial.println("Waiting for CodeCell internal _charge_state to initialize...");
  
  // Wait for power readings to stabilize through Run() cycles
  int stabilizationCycles = 0;
  bool stabilized = false;
  
  myCodeCell.LED(255, 255, 0); // Yellow = stabilizing
  
  while (!stabilized && stabilizationCycles < 20) {
    if (myCodeCell.Run(5)) {  // Run at 5Hz during stabilization
      stabilizationCycles++;
      
      uint8_t powerState = myCodeCell.PowerStateRead();
      uint8_t batteryLevel = myCodeCell.BatteryLevelRead();
      uint16_t batteryVoltage = myCodeCell.BatteryVoltageRead();
      
      Serial.printf("Stabilization cycle %d: PowerState=%d, BatteryLevel=%d, Voltage=%d mV\n", 
                    stabilizationCycles, powerState, batteryLevel, batteryVoltage);
      
      // Check if stabilized (no longer initializing and battery level is valid)
      if (powerState != 2 && batteryLevel != 255) {
        Serial.printf("✅ Power readings stabilized after %d cycles!\n", stabilizationCycles);
        stabilized = true;
        powerSystemWorking = true;
        
        lastPowerState = powerState;
        lastBatteryLevel = batteryLevel;
        lastBatteryVoltage = batteryVoltage;
        
        myCodeCell.LED(0, 255, 0); // Green = stabilized
        delay(1000);
        
        // Now analyze the stabilized values
        Serial.println("\n--- Stabilized Power State Analysis ---");
        Serial.printf("Final: PowerState=%d, BatteryLevel=%d, Voltage=%d mV\n", 
                      powerState, batteryLevel, batteryVoltage);
        
        Serial.print("Power State Meaning: ");
        switch (powerState) {
          case 0: Serial.println("Running from LiPo Battery Power"); break;
          case 1: Serial.println("Running from USB Power"); break;
          case 2: Serial.println("Power Status is Initializing"); break;
          case 3: Serial.println("LiPo Battery is low"); break;
          case 4: Serial.println("LiPo Battery has fully charged"); break;
          case 5: Serial.println("LiPo Battery is charging"); break;
          default: Serial.printf("UNKNOWN/ERROR (%d)\n", powerState); break;
        }
        
        Serial.print("Battery Level Meaning: ");
        if (batteryLevel >= 1 && batteryLevel <= 100) {
          Serial.printf("Battery at %d%%\n", batteryLevel);
        } else if (batteryLevel == 101) {
          Serial.println("Battery is charging");
        } else if (batteryLevel == 102) {
          Serial.println("USB power only (no battery)");
        } else {
          Serial.printf("UNEXPECTED (%d)\n", batteryLevel);
        }
        
        // Check for expected combinations
        if (powerState == 1 && batteryLevel == 102) {
          Serial.println("✅ PERFECT: USB power detected correctly");
        } else if (powerState == 0 && batteryLevel >= 1 && batteryLevel <= 100) {
          Serial.println("✅ PERFECT: Battery power detected correctly");
        } else if (powerState == 5 && batteryLevel == 101) {
          Serial.println("✅ PERFECT: Charging detected correctly");
        } else {
          Serial.println("⚠️ UNEXPECTED: Power state and battery level combination");
        }
        
      } else {
        // Still waiting for stabilization
        myCodeCell.LED(255, 255, 0); // Keep yellow
        delay(100);
      }
    }
    delay(100);
  }
  
  if (!stabilized) {
    Serial.println("❌ TIMEOUT: Power readings never stabilized");
    Serial.printf("Last readings: PowerState=%d, BatteryLevel=%d\n", 
                  myCodeCell.PowerStateRead(), myCodeCell.BatteryLevelRead());
    powerSystemWorking = false;
    myCodeCell.LED(255, 0, 0); // Red = failed
  }
}

void monitorPowerSystemDetailed() {
  // Read current power state
  uint8_t currentPowerState = myCodeCell.PowerStateRead();
  uint16_t currentVoltage = myCodeCell.BatteryVoltageRead();
  uint8_t currentLevel = myCodeCell.BatteryLevelRead();
  
  // Update if values changed
  if (currentPowerState != lastPowerState || 
      abs((int)currentVoltage - (int)lastBatteryVoltage) > 100 ||
      currentLevel != lastBatteryLevel) {
    
    Serial.println("\n--- Power State Change Detected ---");
    Serial.printf("OLD: State=%d, Level=%d, Voltage=%d mV\n", 
                  lastPowerState, lastBatteryLevel, lastBatteryVoltage);
    Serial.printf("NEW: State=%d, Level=%d, Voltage=%d mV\n", 
                  currentPowerState, currentLevel, currentVoltage);
    
    lastPowerState = currentPowerState;
    lastBatteryVoltage = currentVoltage;
    lastBatteryLevel = currentLevel;
    
    // Flash white to indicate change detected
    myCodeCell.LED(255, 255, 255);
    delay(100);
    myCodeCell.LED(0, 0, 0);
    delay(100);
  }
}

void displayPowerState() {
  if (!powerSystemWorking) {
    // Power system failed - fast red blink
    myCodeCell.LED(255, 0, 0);
    delay(100);
    myCodeCell.LED(0, 0, 0);
    delay(100);
    return;
  }
  
  // Display power state via color (per documentation)
  switch (lastPowerState) {
    case 0:  // POWER_BAT_RUN
      myCodeCell.LED(0, 255, 0);   // Green = battery running
      break;
    case 1:  // POWER_USB  
      myCodeCell.LED(0, 0, 255);   // Blue = USB powered
      break;
    case 2:  // POWER_INIT
      myCodeCell.LED(255, 255, 0); // Yellow = initializing
      break;
    case 3:  // POWER_BAT_LOW
      myCodeCell.LED(255, 165, 0); // Orange = low battery
      break;
    case 4:  // POWER_BAT_FULL
      myCodeCell.LED(0, 255, 255); // Cyan = battery full
      break;
    case 5:  // POWER_BAT_CHRG
      myCodeCell.LED(255, 0, 255); // Magenta = charging
      break;
    default:
      myCodeCell.LED(255, 0, 0);   // Red = unknown state
      break;
  }
}

void displayPowerResults() {
  unsigned long totalBootTime = millis() - bootStartTime;
  unsigned long powerTestTime = millis() - powerInitTime;
  
  Serial.println("\n--- Test Results ---");
  Serial.printf("Total boot time: %lu ms\n", totalBootTime);
  Serial.printf("Power test duration: %lu ms\n", powerTestTime);
  Serial.printf("Power system working: %s\n", powerSystemWorking ? "YES" : "NO");
  
  myCodeCell.LED(0, 0, 0);
  delay(1000);
  
  if (powerSystemWorking) {
    Serial.println("✅ Power management test PASSED");
    // Success pattern - 3 green blinks
    for (int i = 0; i < 3; i++) {
      myCodeCell.LED(0, 255, 0);
      delay(300);
      myCodeCell.LED(0, 0, 0);
      delay(300);
    }
  } else {
    Serial.println("❌ Power management test FAILED");
    // Failure pattern - 5 red blinks
    for (int i = 0; i < 5; i++) {
      myCodeCell.LED(255, 0, 0);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  }
  
  Serial.println("\nContinuous monitoring active...");
  Serial.println("Try plugging/unplugging USB to see power state changes");
}