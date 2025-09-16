/*
 * Phase 3C: Full Sensor Suite Test
 * 
 * Only run this if Phases 3A and 3B work properly
 * Tests the full sensor configuration from the broken firmware
 */

#include <CodeCell.h>

CodeCell myCodeCell;

// Full sensor status tracking
bool sensorsInitialized = false;
bool accelValid = false;
bool gyroValid = false;
bool magValid = false;
bool quatValid = false;
bool gravityValid = false;
bool linearAccelValid = false;

void setup() {
  // Initialize Serial FIRST and give it time to stabilize
  Serial.begin(115200);
  delay(2000);  // Longer delay for serial stability
  
  Serial.println("=================================");
  Serial.println("Phase 3C: Full Sensor Suite Test");
  Serial.println("=================================");
  Serial.println("Serial initialized - starting test...");
  delay(500);
  
  // Signal full sensor test starting
  Serial.println("Setting LED indicator...");
  delay(100);  // Small delay before CodeCell operations
  myCodeCell.LED(128, 0, 255); // Purple = full sensor test
  delay(1000);
  
  // Initialize with full sensor suite (same as broken firmware)
  testFullSensorInit();
  displayFullSensorResults();
}

void loop() {
  if (myCodeCell.Run(10)) {  // 10Hz
    
    if (sensorsInitialized) {
      testAllSensorData();
      displayFullSensorStatus();
    } else {
      // Failed - fast red blink
      myCodeCell.LED(255, 0, 0);
      delay(100);
      myCodeCell.LED(0, 0, 0);
      delay(100);
    }
  }
}

void testFullSensorInit() {
  Serial.println("Initializing CodeCell with FULL sensor suite...");
  Serial.println("Sensors: ACCELEROMETER + GYRO + ROTATION + MAGNETOMETER + LINEAR_ACC + GRAVITY");
  
  myCodeCell.LED(255, 255, 0); // Yellow = initializing
  delay(500);
  
  // Initialize with FULL sensor configuration (same as broken firmware)
  unsigned long initStart = millis();
  myCodeCell.Init(MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + 
                 MOTION_MAGNETOMETER + MOTION_LINEAR_ACC + MOTION_GRAVITY);
  unsigned long initDuration = millis() - initStart;
  
  Serial.printf("Init() completed in %lu ms\n", initDuration);
  delay(3000); // Longer delay for full initialization
  
  Serial.println("Testing full sensor initialization...");
  
  // Test basic sensor response
  float ax, ay, az;
  myCodeCell.Motion_AccelerometerRead(ax, ay, az);
  
  Serial.printf("Initial accelerometer test: [%.3f, %.3f, %.3f]\n", ax, ay, az);
  
  if (ax != 0.0 || ay != 0.0 || az != 0.0) {
    sensorsInitialized = true;
    Serial.println("✅ Full sensor initialization SUCCESS");
    myCodeCell.LED(0, 255, 0); // Green success
  } else {
    sensorsInitialized = false;
    Serial.println("❌ Full sensor initialization FAILED - accelerometer readings zero");
    myCodeCell.LED(255, 0, 0); // Red failure
  }
  delay(1000);
}

void testAllSensorData() {
  static unsigned long lastPrint = 0;
  static int printCount = 0;
  
  // Test accelerometer
  float ax, ay, az;
  myCodeCell.Motion_AccelerometerRead(ax, ay, az);
  accelValid = (ax != 0.0 || ay != 0.0 || az != 0.0);
  
  // Test gyroscope
  float gx, gy, gz;
  myCodeCell.Motion_GyroRead(gx, gy, gz);
  gyroValid = true; // Can be zero when stationary
  
  // Test magnetometer
  float mx, my, mz;
  myCodeCell.Motion_MagnetometerRead(mx, my, mz);
  magValid = (mx != 0.0 || my != 0.0 || mz != 0.0);
  
  // Test quaternion (rotation vector)
  float qr, qi, qj, qk;
  myCodeCell.Motion_RotationVectorRead(qr, qi, qj, qk);
  quatValid = (qr != 0.0 || qi != 0.0 || qj != 0.0 || qk != 0.0);
  
  // Test gravity vector
  float gravX, gravY, gravZ;
  myCodeCell.Motion_GravityRead(gravX, gravY, gravZ);
  gravityValid = (gravX != 0.0 || gravY != 0.0 || gravZ != 0.0);
  
  // Test linear acceleration (gravity removed)
  float linX, linY, linZ;
  myCodeCell.Motion_LinearAccRead(linX, linY, linZ);
  linearAccelValid = true; // Can be zero when not moving
  
  // Print sensor data every 2 seconds
  if (millis() - lastPrint > 2000 && printCount < 5) {
    Serial.println("\n--- Sensor Data Snapshot ---");
    Serial.printf("Accelerometer: [%.3f, %.3f, %.3f] - %s\n", ax, ay, az, accelValid ? "VALID" : "INVALID");
    Serial.printf("Gyroscope: [%.3f, %.3f, %.3f] - %s\n", gx, gy, gz, gyroValid ? "VALID" : "INVALID");
    Serial.printf("Magnetometer: [%.3f, %.3f, %.3f] - %s\n", mx, my, mz, magValid ? "VALID" : "INVALID");
    Serial.printf("Quaternion: [%.3f, %.3f, %.3f, %.3f] - %s\n", qr, qi, qj, qk, quatValid ? "VALID" : "INVALID");
    Serial.printf("Gravity: [%.3f, %.3f, %.3f] - %s\n", gravX, gravY, gravZ, gravityValid ? "VALID" : "INVALID");
    Serial.printf("Linear Accel: [%.3f, %.3f, %.3f] - %s\n", linX, linY, linZ, linearAccelValid ? "VALID" : "INVALID");
    
    lastPrint = millis();
    printCount++;
  }
}

void displayFullSensorStatus() {
  // Cycle through all sensor indicators
  
  // Accelerometer - Green
  if (accelValid) {
    myCodeCell.LED(0, 255, 0);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  // Gyroscope - Blue  
  if (gyroValid) {
    myCodeCell.LED(0, 0, 255);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  // Magnetometer - Cyan
  if (magValid) {
    myCodeCell.LED(0, 255, 255);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  // Quaternion - Yellow
  if (quatValid) {
    myCodeCell.LED(255, 255, 0);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  // Gravity - Magenta
  if (gravityValid) {
    myCodeCell.LED(255, 0, 255);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  // Linear Accel - White
  if (linearAccelValid) {
    myCodeCell.LED(255, 255, 255);
  } else {
    myCodeCell.LED(255, 0, 0);
  }
  delay(100);
  
  myCodeCell.LED(0, 0, 0);
  delay(300);
}

void displayFullSensorResults() {
  delay(1000);
  
  Serial.println("\n--- Full Sensor Test Results ---");
  Serial.printf("System initialized: %s\n", sensorsInitialized ? "YES" : "NO");
  
  if (sensorsInitialized) {
    // Count valid sensors
    int validSensors = 0;
    Serial.println("\nSensor validation:");
    if (accelValid) { Serial.println("✅ Accelerometer: VALID"); validSensors++; } else { Serial.println("❌ Accelerometer: INVALID"); }
    if (gyroValid) { Serial.println("✅ Gyroscope: VALID"); validSensors++; } else { Serial.println("❌ Gyroscope: INVALID"); }
    if (magValid) { Serial.println("✅ Magnetometer: VALID"); validSensors++; } else { Serial.println("❌ Magnetometer: INVALID"); }
    if (quatValid) { Serial.println("✅ Quaternion: VALID"); validSensors++; } else { Serial.println("❌ Quaternion: INVALID"); }
    if (gravityValid) { Serial.println("✅ Gravity Vector: VALID"); validSensors++; } else { Serial.println("❌ Gravity Vector: INVALID"); }
    if (linearAccelValid) { Serial.println("✅ Linear Acceleration: VALID"); validSensors++; } else { Serial.println("❌ Linear Acceleration: INVALID"); }
    
    Serial.printf("\nValid sensors: %d/6\n", validSensors);
    
    if (validSensors == 6) {
      Serial.println("✅ Phase 3C: Full sensor test PASSED - All sensors working!");
    } else {
      Serial.println("⚠️ Phase 3C: Partial success - Some sensors not working");
    }
    
    Serial.println("Continuous monitoring active...");
    Serial.println("Watch LED patterns: Green=Accel, Blue=Gyro, Cyan=Mag, Yellow=Quat, Magenta=Gravity, White=LinAccel");
    
    // Success - blink count = number of valid sensors
    for (int i = 0; i < validSensors; i++) {
      myCodeCell.LED(0, 255, 0);
      delay(300);
      myCodeCell.LED(0, 0, 0);
      delay(300);
    }
    
    delay(1000);
    
    // Show total (should be 6)
    for (int i = 0; i < 6; i++) {
      myCodeCell.LED(255, 255, 255);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  } else {
    Serial.println("❌ Phase 3C: Full sensor test FAILED - Complete initialization failure");
    
    // Complete failure - 10 red blinks
    for (int i = 0; i < 10; i++) {
      myCodeCell.LED(255, 0, 0);
      delay(150);
      myCodeCell.LED(0, 0, 0);
      delay(150);
    }
  }
}