/*
 * Phase 3A: Single Sensor Test - Accelerometer Only
 * 
 * Tests BNO085 with minimal sensor configuration
 * This is the most likely failure point based on original issue
 * 
 * Start with just accelerometer to isolate sensor initialization problems
 */

#include <CodeCell.h>

CodeCell myCodeCell;

// Timing and status tracking
unsigned long bootStartTime;
unsigned long sensorInitTime;
bool sensorInitialized = false;
bool sensorDataValid = false;
float lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
int consecutiveValidReadings = 0;

void setup() {
  // Initialize Serial FIRST (like working power test)
  Serial.begin(115200);
  delay(1000);
  
  bootStartTime = millis();
  
  Serial.println("=================================");
  Serial.println("Phase 3A: Single Sensor Test");
  Serial.println("=================================");
  Serial.printf("Boot start time: %lu ms\n", bootStartTime);
  
  // Signal start of sensor test
  myCodeCell.LED(255, 0, 255); // Magenta = sensor test starting
  delay(1000);
  
  sensorInitTime = millis();
  
  // Initialize with ONLY accelerometer
  testSensorInitialization();
  
  // Show results
  displaySensorResults();
}

void loop() {
  if (myCodeCell.Run(10)) {  // 10Hz rate for sensor testing
    
    if (sensorInitialized) {
      // Test sensor data reading
      testSensorData();
      
      // Display sensor status
      displaySensorStatus();
    } else {
      // Sensor failed - show failure pattern
      myCodeCell.LED(255, 0, 0); // Red
      delay(100);
      myCodeCell.LED(0, 0, 0);
      delay(100);
    }
  }
}

void testSensorInitialization() {
  Serial.println("Initializing CodeCell with ACCELEROMETER ONLY...");
  
  // Signal sensor init starting
  myCodeCell.LED(255, 255, 0); // Yellow = initializing
  delay(500);
  
  // Try to initialize with ONLY accelerometer
  unsigned long initStart = millis();
  myCodeCell.Init(MOTION_ACCELEROMETER);
  unsigned long initDuration = millis() - initStart;
  
  Serial.printf("Init() completed in %lu ms\n", initDuration);
  delay(2000); // Give time for initialization
  
  // Test if initialization worked by reading sensor
  Serial.println("Testing accelerometer readings...");
  float testAx, testAy, testAz;
  myCodeCell.Motion_AccelerometerRead(testAx, testAy, testAz);
  
  Serial.printf("Accelerometer test: [%.3f, %.3f, %.3f]\n", testAx, testAy, testAz);
  
  // Check if we got valid data (non-zero values expected due to gravity)
  if (testAx != 0.0 || testAy != 0.0 || testAz != 0.0) {
    sensorInitialized = true;
    lastAccelX = testAx;
    lastAccelY = testAy;
    lastAccelZ = testAz;
    
    Serial.println("✅ Accelerometer initialization SUCCESS");
    
    // Success signal
    myCodeCell.LED(0, 255, 0); // Green
    delay(500);
  } else {
    sensorInitialized = false;
    
    Serial.println("❌ Accelerometer initialization FAILED - all readings zero");
    
    // Failure signal
    myCodeCell.LED(255, 0, 0); // Red
    delay(500);
  }
}

void testSensorData() {
  float ax, ay, az;
  myCodeCell.Motion_AccelerometerRead(ax, ay, az);
  
  // Check if data is valid and changing
  bool dataValid = (ax != 0.0 || ay != 0.0 || az != 0.0);
  bool dataChanged = (abs(ax - lastAccelX) > 0.01 || 
                     abs(ay - lastAccelY) > 0.01 || 
                     abs(az - lastAccelZ) > 0.01);
  
  if (dataValid) {
    consecutiveValidReadings++;
    sensorDataValid = true;
    
    // Update last values
    lastAccelX = ax;
    lastAccelY = ay;
    lastAccelZ = az;
    
    // Flash blue when data changes (movement detected)
    if (dataChanged) {
      myCodeCell.LED(0, 0, 255);
      delay(50);
      myCodeCell.LED(0, 0, 0);
      delay(50);
    }
  } else {
    consecutiveValidReadings = 0;
    sensorDataValid = false;
  }
}

void displaySensorStatus() {
  if (!sensorInitialized) {
    // Not initialized - fast red blink
    myCodeCell.LED(255, 0, 0);
    delay(100);
    myCodeCell.LED(0, 0, 0);
    delay(100);
    return;
  }
  
  if (!sensorDataValid) {
    // Initialized but no valid data - orange blink
    myCodeCell.LED(255, 165, 0);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
    return;
  }
  
  // Working properly - green with intensity based on acceleration magnitude
  float magnitude = sqrt(lastAccelX*lastAccelX + lastAccelY*lastAccelY + lastAccelZ*lastAccelZ);
  
  // Scale magnitude to LED brightness (typical gravity ~9.8 m/s^2)
  int brightness = constrain((int)(magnitude * 25), 10, 255);
  
  myCodeCell.LED(0, brightness, 0);
  delay(100);
  myCodeCell.LED(0, 0, 0);
  delay(100);
}

void displaySensorResults() {
  unsigned long totalBootTime = millis() - bootStartTime;
  unsigned long sensorInitDuration = millis() - sensorInitTime;
  
  myCodeCell.LED(0, 0, 0);
  delay(1000);
  
  if (sensorInitialized) {
    // Success - 3 green blinks
    for (int i = 0; i < 3; i++) {
      myCodeCell.LED(0, 255, 0);
      delay(400);
      myCodeCell.LED(0, 0, 0);
      delay(400);
    }
  } else {
    // Failure - 5 red blinks  
    for (int i = 0; i < 5; i++) {
      myCodeCell.LED(255, 0, 0);
      delay(200);
      myCodeCell.LED(0, 0, 0);
      delay(200);
    }
  }
  
  delay(1000);
  
  // Show total boot time (white blinks)
  int bootSeconds = totalBootTime / 1000;
  for (int i = 0; i < bootSeconds && i < 10; i++) {
    myCodeCell.LED(255, 255, 255);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
  }
  
  delay(1000);
  
  // Show sensor init time (cyan blinks)
  int initSeconds = sensorInitDuration / 1000;
  for (int i = 0; i < initSeconds && i < 10; i++) {
    myCodeCell.LED(0, 255, 255);
    delay(200);
    myCodeCell.LED(0, 0, 0);
    delay(200);
  }
  
  delay(2000);
}