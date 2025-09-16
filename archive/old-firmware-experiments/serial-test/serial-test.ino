#include <CodeCell.h>

CodeCell myCodeCell;
float motionThreshold = 0.4; // Motion threshold in g

void setup() {
  Serial.begin(115200);
  
  // ESP32C3 specific: Wait for USB CDC connection
  while (!Serial) {
    delay(10);
  }
  
  delay(1000); // Extra delay for ESP32C3 USB CDC
  
  Serial.println("========================================");
  Serial.println("CodeCell Motion Sleep Test with Debug");
  Serial.println("========================================");

  delay(60); // Waking up from Sleep - add a small delay for Serial
  
  Serial.println("Checking wake-up status...");
  if (myCodeCell.WakeUpCheck()) {
    Serial.println("WakeUpCheck() returned TRUE - waking from sleep");
    
    // Initialize motion sensor
    Serial.println("Initializing motion sensor...");
    myCodeCell.Motion_Init(MOTION_ACCELEROMETER);
    delay(1000); // Give sensor time to initialize
    Serial.println("Motion sensor initialized");
    
    delay(500); // Give sensor time to stabilize
    Serial.println("Reading motion baseline...");
    
    // Get baseline motion reading
    float baselineAx, baselineAy, baselineAz;
    myCodeCell.Motion_AccelerometerRead(baselineAx, baselineAy, baselineAz);
    Serial.printf("Baseline motion: [%.3f, %.3f, %.3f]\n", baselineAx, baselineAy, baselineAz);
    
    // Check for motion over 5 samples
    bool motionDetected = false;
    for (int i = 0; i < 5; i++) {
      delay(200);
      myCodeCell.Run(1);
      
      float ax, ay, az;
      myCodeCell.Motion_AccelerometerRead(ax, ay, az);
      
      float deltaAx = abs(ax - baselineAx);
      float deltaAy = abs(ay - baselineAy);
      float deltaAz = abs(az - baselineAz);
      float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
      
      Serial.printf("Motion check %d: delta %.3f (threshold %.3f)\n", i+1, totalDelta, motionThreshold);
      
      if (totalDelta > motionThreshold) {
        motionDetected = true;
        Serial.println("Motion detected during wake-up check!");
        break;
      }
    }
    
    if (!motionDetected) {
      Serial.println("No motion detected - going back to sleep for 1 second");
      myCodeCell.Sleep(1); // If motion is not detected, go back to sleep & check again after 1 sec
    } else {
      Serial.println("Motion detected - staying awake");
    }
  } else {
    Serial.println("WakeUpCheck() returned FALSE - normal startup");
  }

  Serial.println("Initializing all CodeCell peripherals...");
  myCodeCell.Init(MOTION_ACCELEROMETER); // Time to wake up - Initializes all CodeCell peripherals
  Serial.println("Setup complete - entering main loop");
  Serial.println("========================================");
}

void loop() {
  static unsigned long lastCheck = 0;
  static int loopCounter = 0;
  static float baselineAx = 0, baselineAy = 0, baselineAz = 0;
  static bool baselineSet = false;
  
  if (myCodeCell.Run(10)) {  // Run every 10Hz
    loopCounter++;
    unsigned long currentTime = millis();
    
    // Set baseline on first loop iteration
    if (!baselineSet) {
      myCodeCell.Motion_AccelerometerRead(baselineAx, baselineAy, baselineAz);
      baselineSet = true;
      Serial.printf("Loop baseline set: [%.3f, %.3f, %.3f]\n", baselineAx, baselineAy, baselineAz);
    }
    
    float ax, ay, az;
    myCodeCell.Motion_AccelerometerRead(ax, ay, az);
    
    float deltaAx = abs(ax - baselineAx);
    float deltaAy = abs(ay - baselineAy);
    float deltaAz = abs(az - baselineAz);
    float totalDelta = sqrt(deltaAx*deltaAx + deltaAy*deltaAy + deltaAz*deltaAz);
    
    // Print debug info every 5 seconds
    if (currentTime - lastCheck >= 5000) {
      lastCheck = currentTime;
      Serial.printf("Loop #%d - Motion Delta: %.3f, Uptime: %lu seconds\n", 
                   loopCounter, totalDelta, currentTime / 1000);
    }
    
    if (totalDelta < motionThreshold) {
      Serial.printf("No motion detected (%.3f) - going to sleep for 1 second\n", totalDelta);
      Serial.flush();
      myCodeCell.Sleep(1); // If no motion is detected, go to sleep & check again after 1 sec
      Serial.println("Woke up from sleep - continuing loop");
      // Reset baseline after sleep
      baselineSet = false;
    }
  }
}
