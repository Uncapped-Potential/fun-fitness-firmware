/*
 * Phase 2: Power Management Integration Test
 * 
 * Tests power management functions that control the green LED
 * This isolates if the 5-second boot delay and missing green LED
 * are caused by power management system failure
 * 
 * Expected behavior:
 * - Boot within 1-2 seconds  
 * - Green LED activates (via PowerStateRead)
 * - Battery level readings work
 * - USB detection works
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
  // Initialize Serial FIRST and give it time to stabilize
  Serial.begin(115200);
  delay(2000);  // Longer delay for serial stability
  
  Serial.println("=================================");
  Serial.println("Phase 2: Power Management Test");
  Serial.println("=================================");
  Serial.println("Serial initialized - starting test...");
  delay(500);
  
  bootStartTime = millis();
  Serial.printf("Boot start time: %lu ms\n", bootStartTime);
  
  // Separate CodeCell initialization
  Serial.println("Initializing CodeCell (no sensors)...");
  delay(100);  // Small delay before CodeCell operations
  myCodeCell.Init(MOTION_DISABLE);
  
  powerInitTime = millis();
  Serial.printf("CodeCell init completed at: %lu ms\n", powerInitTime);
  
  // Signal start of power test
  myCodeCell.LED(0, 255, 255); // Cyan = power test starting
  delay(1000);
  
  // Test power management functions
  testPowerManagement();
  
  // Show results
  displayPowerResults();
}

void loop() {
  if (myCodeCell.Run(2)) {  // 2Hz rate
    
    // Continuously monitor power system
    monitorPowerSystem();
    
    // Display current power state via LED
    displayPowerState();
    
    delay(500);
  }
}

void testPowerManagement() {
  Serial.println("\n--- Power Management Tests ---");
  
  // Test 1: Basic PowerStateRead (controls green LED)
  Serial.println("Test 1: PowerStateRead...");
  myCodeCell.LED(255, 255, 0); // Yellow = testing power state
  delay(200);
  
  uint8_t powerState = myCodeCell.PowerStateRead();
  lastPowerState = powerState;
  
  Serial.printf("PowerState: %d", powerState);
  switch (powerState) {
    case 0: Serial.println(" (Running from LiPo Battery)"); break;
    case 1: Serial.println(" (Running from USB Power)"); break;
    case 2: Serial.println(" (Power Status Initializing)"); break;
    case 3: Serial.println(" (LiPo Battery Low)"); break;
    case 4: Serial.println(" (LiPo Battery Full)"); break;
    case 5: Serial.println(" (LiPo Battery Charging)"); break;
    default: Serial.printf(" (UNKNOWN/ERROR)\n"); break;
  }
  
  if (powerState != 255) {  // Valid reading
    powerSystemWorking = true;
    Serial.println("✅ PowerState test PASSED");
    myCodeCell.LED(0, 255, 0); // Green = power state OK
  } else {
    Serial.println("❌ PowerState test FAILED");
    myCodeCell.LED(255, 0, 0); // Red = power state failed
  }
  delay(500);
  
  // Test 2: Battery voltage reading
  Serial.println("Test 2: BatteryVoltageRead...");
  myCodeCell.LED(255, 255, 0); // Yellow = testing battery voltage
  delay(200);
  
  uint16_t batteryVoltage = myCodeCell.BatteryVoltageRead();
  lastBatteryVoltage = batteryVoltage;
  
  Serial.printf("Battery Voltage: %d mV\n", batteryVoltage);
  
  if (batteryVoltage > 3000 && batteryVoltage < 5000) {  // Reasonable range
    Serial.println("✅ Battery voltage test PASSED");
    myCodeCell.LED(0, 255, 0); // Green = battery voltage OK
  } else {
    Serial.println("❌ Battery voltage test FAILED - outside expected range");
    myCodeCell.LED(255, 0, 0); // Red = battery voltage failed
    powerSystemWorking = false;
  }
  delay(500);
  
  // Test 3: Battery level reading
  Serial.println("Test 3: BatteryLevelRead...");
  myCodeCell.LED(255, 255, 0); // Yellow = testing battery level
  delay(200);
  
  uint8_t batteryLevel = myCodeCell.BatteryLevelRead();
  lastBatteryLevel = batteryLevel;
  
  Serial.printf("Battery Level: %d", batteryLevel);
  if (batteryLevel >= 1 && batteryLevel <= 100) {
    Serial.printf(" (Battery at %d%%)\n", batteryLevel);
  } else if (batteryLevel == 101) {
    Serial.println(" (Battery is charging)");
  } else if (batteryLevel == 102) {
    Serial.println(" (USB power only - no battery)");
  } else {
    Serial.printf(" (UNEXPECTED VALUE)\n");
  }
  
  if (batteryLevel <= 102) {  // Valid range (0-100, or 101/102 for charging/USB)
    Serial.println("✅ Battery level test PASSED");
    myCodeCell.LED(0, 255, 0); // Green = battery level OK
  } else {
    Serial.println("❌ Battery level test FAILED - unexpected value");
    myCodeCell.LED(255, 0, 0); // Red = battery level failed
    powerSystemWorking = false;
  }
  delay(500);
}

void monitorPowerSystem() {
  // Read current power state
  uint8_t currentPowerState = myCodeCell.PowerStateRead();
  uint16_t currentVoltage = myCodeCell.BatteryVoltageRead();
  uint8_t currentLevel = myCodeCell.BatteryLevelRead();
  
  // Update if values changed
  if (currentPowerState != lastPowerState || 
      abs((int)currentVoltage - (int)lastBatteryVoltage) > 100 ||
      currentLevel != lastBatteryLevel) {
    
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
  
  // Display power state via color
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
    Serial.println("✅ Phase 2: Power management test PASSED");
    Serial.println("Continuous monitoring active...");
    Serial.println("Try plugging/unplugging USB to see power state changes");
    
    // Success pattern - 3 green blinks
    for (int i = 0; i < 3; i++) {
      myCodeCell.LED(0, 255, 0);
      delay(300);
      myCodeCell.LED(0, 0, 0);
      delay(300);
    }
  } else {
    Serial.println("❌ Phase 2: Power management test FAILED");
    
    // Failure pattern - 5 red blinks
    for (int i = 0; i < 5; i++) {
      myCodeCell.LED(255, 0, 0);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  }
  
  delay(1000);
  
  // Show boot time in seconds (white blinks)
  int bootSeconds = totalBootTime / 1000;
  for (int i = 0; i < bootSeconds && i < 10; i++) {
    myCodeCell.LED(255, 255, 255);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
  }
  
  delay(2000);
}