/*
 * Basic Hardware Test - ESP32C3 CodeCell Board
 * Tests basic functionality without CodeCell library
 * Use this to isolate hardware vs firmware issues
 */

#define LED_PIN 10

void setup() {
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  
  // Start with LED off
  digitalWrite(LED_PIN, LOW);
  
  // Short delay to ensure setup completes
  delay(100);
}

void loop() {
  // Simple blink pattern to show the system is running
  
  // Turn LED on (red)
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  
  // Turn LED off
  digitalWrite(LED_PIN, LOW);
  delay(200);
  
  // Repeat pattern 3 times quickly
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Longer pause before repeating
  delay(1000);
}