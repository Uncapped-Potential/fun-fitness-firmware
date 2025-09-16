#ifndef CONFIG_H
#define CONFIG_H

// Device Configuration
#define DEVICE_NAME "FitChip011"

// BLE Service UUIDs (matching MicroLink for compatibility)
#define MAIN_SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define QUATERNION_CHAR_UUID     "dcba4330-dcba-4321-dcba-432123456791"
#define CONFIG_CHAR_UUID         "dcba4330-dcba-4321-dcba-432123456792"

// OTA Service UUIDs
#define OTA_SERVICE_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001"
#define OTA_CONTROL_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0002"
#define OTA_DATA_UUID      "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0003"

// OTA Configuration
#define OTA_MAX_CHUNK_SIZE     128  // Conservative chunk size
#define OTA_QUEUE_SIZE         10   // Number of chunks to queue
#define OTA_TIMEOUT_MS         5000 // Timeout for OTA operations

// Power Management
#define MOTION_THRESHOLD       0.4f   // g-force threshold for motion detection
#define MOTION_REQUIRED_COUNT  2      // Consecutive readings for motion confirmation
#define BLE_INACTIVITY_TIMEOUT 10000  // ms before BLE_OFF state
#define LIGHT_SLEEP_TIMEOUT    60000  // ms before LIGHT_SLEEP state

// Data Rates (Hz)
#define ACTIVE_DATA_RATE       60
#define ACTIVE_IMU_RATE        60
#define LOW_POWER_DATA_RATE    1
#define LOW_POWER_IMU_RATE     10
#define SLEEP_IMU_RATE         5

// Sensor Configuration
#define IMU_STABILIZATION_MS   3000  // Time to wait for IMU stabilization
#define FUSION_READY_TIMEOUT   5000  // Timeout waiting for fusion ready

// Logging
#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

// Test Configuration
#ifdef TESTING
#define TEST_MODE_ENABLED 1
#else
#define TEST_MODE_ENABLED 0
#endif

#endif // CONFIG_H