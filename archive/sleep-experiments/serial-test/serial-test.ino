/*
 * Simple Serial Test for CodeCell
 * Tests basic serial communication
 */

#include <CodeCell.h>

CodeCell myCodeCell;

void setup() {
    // Try multiple common baud rates
    Serial.begin(115200);
    
    // Long delay to ensure Serial is ready
    delay(3000);
    
    // Print test messages
    Serial.println("=====================================");
    Serial.println("SERIAL TEST STARTING - 115200 BAUD");
    Serial.println("=====================================");
    Serial.println("");
    Serial.println("If you see this, Serial is working!");
    Serial.println("");
    
    // Initialize CodeCell with minimal features
    Serial.println("Initializing CodeCell...");
    myCodeCell.Init(LIGHT);  // Just light sensor for minimal init
    Serial.println("CodeCell initialized!");
    
    Serial.println("");
    Serial.println("Will print counter every second...");
    Serial.println("");
}

void loop() {
    static int counter = 0;
    
    // Print counter
    Serial.print("Counter: ");
    Serial.println(counter);
    
    // Also print battery level to verify CodeCell is working
    int battery = myCodeCell.BatteryLevelRead();
    Serial.print("Battery Level: ");
    Serial.println(battery);
    
    Serial.println("---");
    
    counter++;
    delay(1000);  // Wait 1 second
}