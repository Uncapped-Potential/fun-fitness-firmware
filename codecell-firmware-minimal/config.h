#ifndef CONFIG_H
#define CONFIG_H

// Device Configuration
#define DEVICE_NAME         "FitChip011"

// BLE Service UUIDs - Single Service with Quaternion + OTA Control
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "dcba4330-dcba-4321-dcba-432123456791"
#define OTA_CONTROL_CHAR_UUID "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0002"

// Sensor Performance Configuration
#define UPDATE_RATE         60    // Hz - optimized for 60Hz performance
#define SLEEP_TIMEOUT       10000 // ms - sleep after 10 seconds of no motion

// Motion Detection Configuration
#define MOTION_THRESHOLD    0.3f  // g-force threshold for motion detection

// Performance-Critical Sensor Configuration
// This exact combination achieves 60Hz accel/gyro + 20-40Hz quaternions
#define SENSOR_CONFIG (MOTION_ACCELEROMETER + MOTION_GYRO + MOTION_ROTATION + MOTION_MAGNETOMETER)

// Timing Configuration (Performance Critical)
#define SENSOR_STABILIZATION_MS  3000  // Initialization delay
#define MAIN_LOOP_DELAY_MS       1     // Minimal cooperative delay
#define DEBUG_INTERVAL_MS        1000  // 1-second debug output

// OTA Worker Configuration (Conservative Settings)
#define OTA_CHUNK_SIZE          128     // Conservative chunk size (bytes)
#define OTA_QUEUE_SIZE          8       // FreeRTOS queue depth
#define OTA_TASK_STACK_SIZE     4096    // Stack size for OTA worker task
#define OTA_TASK_PRIORITY       1       // Low priority (below main loop)
#define OTA_CHUNK_TIMEOUT_MS    30000   // 30-second chunk timeout
#define OTA_SESSION_TIMEOUT_MS  300000  // 5-minute session timeout

#endif // CONFIG_H