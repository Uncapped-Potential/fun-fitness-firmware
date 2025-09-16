/*
 * CodeCell OTA Library Implementation
 * 
 * Isolated Over-The-Air update functionality for CodeCell devices.
 * This library is designed to be stable and independent of main firmware changes.
 * 
 * Version: 1.0
 * Author: Cooper Shea and Claude
 */

#include "ota_library.h"

CodeCellOTA::CodeCellOTA() {
    // Initialize all state variables
    agreedChunk = DEFAULT_PROPOSED_CHUNK;
    update_part = nullptr;
    ota_handle = 0;
    total_len = 0;
    rx_len = 0;
    host_crc = 0;
    ota_active = false;
}

CodeCellOTA::~CodeCellOTA() {
    reset();
}

bool CodeCellOTA::init(BLEServer* server) {
    if (!server) {
        Serial.println("OTA: Invalid BLE server");
        return false;
    }
    
    try {
        // Create OTA service
        BLEService *svc = server->createService(OTA_SERVICE_UUID);
        if (!svc) {
            Serial.println("OTA: Failed to create service");
            return false;
        }

        // Create control characteristic
        pOtaCtrl = svc->createCharacteristic(
            OTA_CONTROL_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
        );
        if (!pOtaCtrl) {
            Serial.println("OTA: Failed to create control characteristic");
            return false;
        }
        pOtaCtrl->addDescriptor(new BLE2902());
        pOtaCtrl->setCallbacks(new OtaControlCallbacks(this));

        // Create data characteristic
        pOtaData = svc->createCharacteristic(
            OTA_DATA_UUID,
            BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE
        );
        if (!pOtaData) {
            Serial.println("OTA: Failed to create data characteristic");
            return false;
        }
        pOtaData->setCallbacks(new OtaDataCallbacks(this));

        // Start the service
        svc->start();
        Serial.println("OTA: Service initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Serial.printf("OTA: Exception during initialization: %s\n", e.what());
        return false;
    } catch (...) {
        Serial.println("OTA: Unknown exception during initialization");
        return false;
    }
}

uint8_t CodeCellOTA::getProgress() const {
    if (total_len == 0) return 0;
    return (uint8_t)((rx_len * 100) / total_len);
}

void CodeCellOTA::reset() {
    if (ota_active && ota_handle != 0) {
        esp_ota_abort(ota_handle);
    }
    
    ota_active = false;
    ota_handle = 0;
    total_len = 0;
    rx_len = 0;
    host_crc = 0;
    update_part = nullptr;
    agreedChunk = DEFAULT_PROPOSED_CHUNK;
}

uint32_t CodeCellOTA::crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

void CodeCellOTA::notifyError(uint8_t code) {
    Serial.printf("OTA: Error code %u\n", code);
    if (pOtaCtrl) {
        uint8_t e[2] = { OP_ERROR, code };
        pOtaCtrl->setValue(e, sizeof(e));
        pOtaCtrl->notify();
    }
    reset(); // Clean up on error
}

// OTA Control Callbacks Implementation
void OtaControlCallbacks::onWrite(BLECharacteristic* c) {
    if (!otaInstance) return;
    
    String arduinoString = c->getValue();
    std::string s(arduinoString.c_str(), arduinoString.length());
    if (s.size() == 0) return;
    
    const uint8_t *buf = (const uint8_t*)s.data();
    uint8_t op = buf[0];

    switch (op) {
        case OP_START:
            if (s.size() >= (1 + 4 + 4 + 2)) {
                if (otaInstance->ota_active) {
                    Serial.println("OTA: Already active, ignoring START");
                    return;
                }
                
                otaInstance->total_len = otaInstance->le32(&buf[1]);
                otaInstance->host_crc = otaInstance->le32(&buf[5]);
                uint16_t proposed = otaInstance->le16(&buf[9]);

                otaInstance->update_part = esp_ota_get_next_update_partition(NULL);
                if (!otaInstance->update_part) {
                    otaInstance->notifyError(1);
                    return;
                }
                
                if (esp_ota_begin(otaInstance->update_part, otaInstance->total_len, &otaInstance->ota_handle) != ESP_OK) {
                    otaInstance->notifyError(2);
                    return;
                }

                otaInstance->rx_len = 0;
                otaInstance->ota_active = true;
                otaInstance->agreedChunk = (proposed >= 20 && proposed <= 200) ? proposed : 200;

                Serial.printf("OTA: START - %u bytes, chunk size: %u\n", otaInstance->total_len, otaInstance->agreedChunk);

                uint8_t ack[3] = { 
                    OP_ACK_START, 
                    (uint8_t)(otaInstance->agreedChunk & 0xFF), 
                    (uint8_t)(otaInstance->agreedChunk >> 8) 
                };
                c->setValue(ack, sizeof(ack));
                c->notify();
            }
            break;
            
        case OP_FINISH:
            {
                if (!otaInstance->ota_active) {
                    Serial.println("OTA: FINISH received but OTA not active");
                    return;
                }
                
                // Validate received length
                if (otaInstance->rx_len != otaInstance->total_len) {
                    Serial.printf("OTA: FINISH ERROR - length mismatch %u/%u\n", otaInstance->rx_len, otaInstance->total_len);
                    otaInstance->notifyError(3);
                    return;
                }
                
                Serial.printf("OTA: FINISH - %u bytes received (CRC check skipped)\n", otaInstance->rx_len);
                
                if (esp_ota_end(otaInstance->ota_handle) != ESP_OK) {
                    otaInstance->notifyError(4);
                    return;
                }
                
                if (esp_ota_set_boot_partition(otaInstance->update_part) != ESP_OK) {
                    otaInstance->notifyError(5);
                    return;
                }
                
                Serial.println("OTA: COMPLETE - ready to reboot");
                uint8_t ack = OP_ACK_FINISH;
                c->setValue(&ack, 1);
                c->notify();
                otaInstance->ota_active = false;
            }
            break;
            
        case OP_REBOOT:
            Serial.println("OTA: REBOOT requested");
            otaInstance->reset();
            delay(50);
            esp_restart();
            break;
            
        default:
            Serial.printf("OTA: Unknown opcode: 0x%02X\n", op);
            break;
    }
}

// OTA Data Callbacks Implementation
void OtaDataCallbacks::onWrite(BLECharacteristic* c) {
    if (!otaInstance || !otaInstance->ota_active) return;
    
    String arduinoString = c->getValue();
    std::string s(arduinoString.c_str(), arduinoString.length());
    if (s.size() < 1 + 4) return;
    
    const uint8_t *buf = (const uint8_t*)s.data();
    if (buf[0] != OP_DATA) return;
    
    uint32_t offset = otaInstance->le32(&buf[1]);
    const uint8_t *payload = &buf[5];
    size_t plen = s.size() - 5;

    // Write chunk to flash at the specified offset
    if (esp_ota_write_with_offset(otaInstance->ota_handle, payload, plen, offset) != ESP_OK) {
        Serial.printf("OTA: WRITE ERROR at offset %u\n", offset);
        otaInstance->notifyError(7);
        return;
    }
    
    // Update received bytes count (track highest offset + length)
    uint32_t chunk_end = offset + plen;
    if (chunk_end > otaInstance->rx_len) {
        otaInstance->rx_len = chunk_end;
    }
    
    Serial.printf("OTA: DATA - Wrote %u bytes at offset %u, total progress: %u/%u\n", 
                  plen, offset, otaInstance->rx_len, otaInstance->total_len);

    // Send progress notifications periodically
    if ((otaInstance->rx_len & 0x3FFF) == 0 || otaInstance->rx_len == otaInstance->total_len) {
        uint8_t prog[1 + 4 + 4];
        prog[0] = OP_PROGRESS;
        otaInstance->put_le32(&prog[1], otaInstance->rx_len);
        otaInstance->put_le32(&prog[5], otaInstance->total_len);
        otaInstance->pOtaCtrl->setValue(prog, sizeof(prog));
        otaInstance->pOtaCtrl->notify();
        Serial.printf("OTA: PROGRESS - %u/%u bytes (%u%%)\n", 
                      otaInstance->rx_len, otaInstance->total_len, otaInstance->getProgress());
    }
}