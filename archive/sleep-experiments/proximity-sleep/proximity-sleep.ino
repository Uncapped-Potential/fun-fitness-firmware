#include <CodeCell.h>

CodeCell myCodeCell;

void setup() {
  Serial.begin(115200); 

  delay(60); // Waking up from Sleep - add a small delay for Serial
  if (myCodeCell.WakeUpCheck()) {
    // Initialize light sensor
    while (myCodeCell.Light_Init() == 1) {
      delay(1);
      myCodeCell.LightReset(); // If sensor not responding, reset it
    }
    delay(40);
    myCodeCell.Light_Read(); // Read value from light sensor
    if (myCodeCell.Light_ProximityRead() < 10) {
      // Flash LED red before sleeping
      myCodeCell.LED(0xFF, 0x00, 0x00);  // Red
      delay(100);
      myCodeCell.LED(0x00, 0x00, 0x00);  // Off
      
      myCodeCell.Sleep(1); // If proximity is still not detected, go back to sleep & check again after 1 sec
    }
  }

  myCodeCell.Init(LIGHT); // Time to wake up - Initializes all CodeCell peripherals
  
  // Flash LED green when awake
  myCodeCell.LED(0x00, 0xFF, 0x00);  // Green
  delay(500);
  myCodeCell.LED(0x00, 0x00, 0x00);  // Off
}

void loop() {
  if (myCodeCell.Run(10)) {  // Run every 10Hz
    int proximity = myCodeCell.Light_ProximityRead();
    
    // LED feedback for proximity
    if (proximity < 10) {
      // Red LED = going to sleep (no proximity)
      myCodeCell.LED(0xFF, 0x00, 0x00);  // Red
      delay(100);
      myCodeCell.LED(0x00, 0x00, 0x00);  // Off
      
      myCodeCell.Sleep(1); // If proximity is not detected, go to sleep & check again after 1 sec
    } else {
      // Blue LED pulse = awake and detecting proximity
      myCodeCell.LED(0x00, 0x00, 0xFF);  // Blue
      delay(50);
      myCodeCell.LED(0x00, 0x00, 0x00);  // Off
    }
  }
}