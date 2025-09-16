/*
 * CodeCell OTA Library
 * 
 * Isolated Over-The-Air update functionality for CodeCell devices.
 * This library is designed to be stable and independent of main firmware changes.
 * 
 * Version: 1.0
 * Author: Cooper Shea and Claude
 */

#ifndef OTA_LIBRARY_H
#define OTA_LIBRARY_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <string>

// OTA Service UUIDs - Fixed and stable
#define OTA_SERVICE_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001"
#define OTA_CONTROL_UUID   "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0002"
#define OTA_DATA_UUID      "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0003"

// OTA Control opcodes
enum OtaOpcodes { 
    OP_START = 0x01, 
    OP_DATA = 0x02, 
    OP_FINISH = 0x03, 
    OP_REBOOT = 0x04,
    OP_ACK_START = 0xA1, 
    OP_ACK_FINISH = 0xA2, 
    OP_PROGRESS = 0x91, 
    OP_ERROR = 0xE0 
};

class CodeCellOTA {
public:
    CodeCellOTA();
    ~CodeCellOTA();
    
    // Initialize OTA service with BLE server
    bool init(BLEServer* server);
    
    // Check if OTA is currently active
    bool isActive() const { return ota_active; }
    
    // Get current progress (0-100%)
    uint8_t getProgress() const;
    
    // Cleanup and reset OTA state
    void reset();

private:
    // BLE characteristics
    BLECharacteristic *pOtaCtrl = nullptr;
    BLECharacteristic *pOtaData = nullptr;
    
    // OTA state variables
    static const uint16_t DEFAULT_PROPOSED_CHUNK = 200;
    uint16_t agreedChunk = 200;
    const esp_partition_t *update_part = nullptr;
    esp_ota_handle_t ota_handle = 0;
    uint32_t total_len = 0;
    uint32_t rx_len = 0;
    uint32_t host_crc = 0;
    bool ota_active = false;
    
    // Helper functions
    static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);
    static inline uint16_t le16(const uint8_t* p) { 
        return (uint16_t)p[0] | (uint16_t)p[1] << 8; 
    }
    static inline uint32_t le32(const uint8_t* p) { 
        return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; 
    }
    static inline void put_le32(uint8_t* d, uint32_t v) { 
        d[0] = v & 0xFF; 
        d[1] = (v >> 8) & 0xFF; 
        d[2] = (v >> 16) & 0xFF; 
        d[3] = (v >> 24) & 0xFF; 
    }
    
    void notifyError(uint8_t code);
    
    // Callback classes as friends
    friend class OtaControlCallbacks;
    friend class OtaDataCallbacks;
};

// BLE callback classes
class OtaControlCallbacks : public BLECharacteristicCallbacks {
public:
    OtaControlCallbacks(CodeCellOTA* ota) : otaInstance(ota) {}
    void onWrite(BLECharacteristic* c) override;
    
private:
    CodeCellOTA* otaInstance;
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
public:
    OtaDataCallbacks(CodeCellOTA* ota) : otaInstance(ota) {}
    void onWrite(BLECharacteristic* c) override;
    
private:
    CodeCellOTA* otaInstance;
};

#endif // OTA_LIBRARY_H