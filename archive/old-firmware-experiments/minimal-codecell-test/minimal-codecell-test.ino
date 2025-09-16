/*
 * Minimal CodeCell Test - Just LED and Power Check
 * Tests basic CodeCell library without sensors or BLE
 * Use this to isolate if the issue is in sensor/BLE initialization
 */

#include <CodeCell.h>

CodeCell myCodeCell;

void setup() {
  // Initialize with NO motion sensors, NO light sensor
  myCodeCell.Init(MOTION_DISABLE);
  
  // Simple delay
  delay(1000);
}

void loop() {
  // Check if CodeCell basic functions work
  if (myCodeCell.Run(1)) {  // 1Hz rate
    
    // Try to read power state (this controls green LED)
    uint8_t powerState = myCodeCell.PowerStateRead();
    
    // Simple blink pattern using CodeCell LED
    myCodeCell.LED(255, 0, 0);  // Red
    delay(500);
    
    myCodeCell.LED(0, 255, 0);  // Green  
    delay(500);
    
    myCodeCell.LED(0, 0, 255);  // Blue
    delay(500);
    
    myCodeCell.LED(0, 0, 0);    // Off
    delay(500);
  }
}