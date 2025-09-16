/*
 * Phase 3B: Dual Sensor Test - Accelerometer + Gyroscope
 * 
 * Only run this if Phase 3A (single sensor) works properly
 * Tests if adding gyroscope causes issues
 */

#include <CodeCell.h>

CodeCell myCodeCell;

// Status tracking  
bool sensorsInitialized = false;
bool accelDataValid = false;
bool gyroDataValid = false;
float lastAccelMagnitude = 0;
float lastGyroMagnitude = 0;

void setup() {
  // Initialize Serial FIRST and give it time to stabilize
  Serial.begin(115200);
  delay(2000);  // Longer delay for serial stability
  
  Serial.println("=================================");
  Serial.println("Phase 3B: Dual Sensor Test");
  Serial.println("=================================");
  Serial.println("Serial initialized - starting test...");
  delay(500);
  
  // Signal dual sensor test starting
  Serial.println("Setting LED indicator...");
  delay(100);  // Small delay before CodeCell operations
  myCodeCell.LED(255, 0, 128); // Pink = dual sensor test
  delay(1000);
  
  // Initialize with accelerometer + gyroscope
  testDualSensorInit();
  displayInitResults();
}

void loop() {
  if (myCodeCell.Run(10)) {  // 10Hz
    
    if (sensorsInitialized) {
      testDualSensorData();
      displayDualSensorStatus();
    } else {
      // Failed - red blink
      myCodeCell.LED(255, 0, 0);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  }
}

void testDualSensorInit() {
  Serial.println("Initializing CodeCell with ACCELEROMETER + GYROSCOPE...");
  
  myCodeCell.LED(255, 255, 0); // Yellow = initializing
  delay(500);
  
  // Initialize with accelerometer + gyroscope
  unsigned long initStart = millis();
  myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO);
  unsigned long initDuration = millis() - initStart;
  
  Serial.printf("Init() completed in %lu ms\n", initDuration);
  delay(2000);
  
  Serial.println("Testing dual sensor readings...");
  
  // Test both sensors
  float ax, ay, az, gx, gy, gz;
  myCodeCell.Motion_AccelerometerRead(ax, ay, az);
  myCodeCell.Motion_GyroRead(gx, gy, gz);
  
  Serial.printf("Accelerometer: [%.3f, %.3f, %.3f] m/s²\n", ax, ay, az);
  Serial.printf("Gyroscope: [%.3f, %.3f, %.3f] rad/s\n", gx, gy, gz);
  
  bool accelWorking = (ax != 0.0 || ay != 0.0 || az != 0.0);
  bool gyroWorking = true; // Gyro can be zero when stationary
  
  if (accelWorking) {
    sensorsInitialized = true;
    Serial.println("✅ Dual sensor initialization SUCCESS");
    myCodeCell.LED(0, 255, 0); // Green success
  } else {
    sensorsInitialized = false;
    Serial.println("❌ Dual sensor initialization FAILED - accelerometer readings zero");
    myCodeCell.LED(255, 0, 0); // Red failure
  }
  delay(500);
}

void testDualSensorData() {
  float ax, ay, az, gx, gy, gz;
  myCodeCell.Motion_AccelerometerRead(ax, ay, az);
  myCodeCell.Motion_GyroRead(gx, gy, gz);
  
  // Calculate magnitudes
  float accelMag = sqrt(ax*ax + ay*ay + az*az);
  float gyroMag = sqrt(gx*gx + gy*gy + gz*gz);
  
  accelDataValid = (accelMag > 0.1); // Should be ~9.8 m/s^2 from gravity
  gyroDataValid = true; // Gyro can be zero when not rotating
  
  lastAccelMagnitude = accelMag;
  lastGyroMagnitude = gyroMag;
}

void displayDualSensorStatus() {
  if (!accelDataValid) {
    // Accelerometer failed - red
    myCodeCell.LED(255, 0, 0);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
    return;
  }
  
  // Both working - alternate green (accel) and blue (gyro)
  // Green intensity based on accelerometer
  int accelBrightness = constrain((int)(lastAccelMagnitude * 25), 10, 255);
  myCodeCell.LED(0, accelBrightness, 0);
  delay(150);
  
  // Blue intensity based on gyroscope (only if rotating)
  int gyroBrightness = constrain((int)(lastGyroMagnitude * 100), 0, 255);
  if (gyroBrightness < 10) gyroBrightness = 50; // Minimum visibility
  myCodeCell.LED(0, 0, gyroBrightness);
  delay(150);
  
  myCodeCell.LED(0, 0, 0);
  delay(200);
}

void displayInitResults() {
  delay(1000);
  
  Serial.println("\n--- Test Results ---");
  Serial.printf("Dual sensor system working: %s\n", sensorsInitialized ? "YES" : "NO");
  
  if (sensorsInitialized) {
    Serial.println("✅ Phase 3B: Dual sensor test PASSED");
    Serial.println("Continuous monitoring active...");
    Serial.println("Try moving the device to see accelerometer/gyroscope data");
    
    // Success - 4 green blinks (dual sensor)
    for (int i = 0; i < 4; i++) {
      myCodeCell.LED(0, 255, 0);
      delay(300);
      myCodeCell.LED(0, 0, 0);
      delay(300);
    }
  } else {
    Serial.println("❌ Phase 3B: Dual sensor test FAILED");
    
    // Failure - 6 red blinks
    for (int i = 0; i < 6; i++) {
      myCodeCell.LED(255, 0, 0);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  }
}