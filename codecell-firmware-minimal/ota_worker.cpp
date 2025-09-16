#include "ota_worker.h"
#include <esp_ota_ops.h>

OtaWorker::OtaWorker()
    : workerTaskHandle(nullptr)
    , chunkQueue(nullptr)
    , initialized(false)
    , nextExpectedOffset(0)
    , sessionStartTime(0)
    , assemblyBuffer(nullptr)
    , assemblyBufferSize(0)
{
    resetOtaState();
}

OtaWorker::~OtaWorker() {
    if (workerTaskHandle) {
        vTaskDelete(workerTaskHandle);
    }
    if (chunkQueue) {
        vQueueDelete(chunkQueue);
    }
    if (assemblyBuffer) {
        free(assemblyBuffer);
    }
}

bool OtaWorker::init() {
    Serial.println("Initializing OTA worker...");

    // Create chunk processing queue (conservative size)
    chunkQueue = xQueueCreate(OTA_QUEUE_SIZE, sizeof(OtaChunk));
    if (!chunkQueue) {
        Serial.println("ERROR: Failed to create OTA chunk queue");
        return false;
    }

    // Create worker task with conservative stack size
    BaseType_t result = xTaskCreate(
        otaWorkerTask,
        "OTA_Worker",
        OTA_TASK_STACK_SIZE,
        this,
        OTA_TASK_PRIORITY,
        &workerTaskHandle
    );

    if (result != pdPASS) {
        Serial.println("ERROR: Failed to create OTA worker task");
        vQueueDelete(chunkQueue);
        chunkQueue = nullptr;
        return false;
    }

    initialized = true;
    Serial.println("OTA worker initialization complete");
    return true;
}

bool OtaWorker::startOta(uint32_t totalSize) {
    if (!initialized) {
        setError("OTA worker not initialized");
        return false;
    }

    if (isActive()) {
        setError("OTA already in progress");
        return false;
    }

    Serial.printf("Starting OTA session: %u bytes\n", totalSize);

    // Check available OTA space
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition) {
        Serial.printf("OTA: Available partition size: %u bytes\n", update_partition->size);
        if (totalSize > update_partition->size) {
            setError("Firmware too large for OTA partition");
            Serial.printf("OTA ERROR: Requested %u bytes, but partition only has %u bytes\n",
                         totalSize, update_partition->size);
            return false;
        }
    } else {
        setError("No OTA partition found");
        return false;
    }

    // Reset state for new session
    resetOtaState();
    status.state = OTA_RECEIVING;
    status.totalSize = totalSize;
    status.expectedChunks = (totalSize + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE;
    sessionStartTime = millis();
    nextExpectedOffset = 0;

    // Initialize ESP32 Update library
    if (!Update.begin(totalSize)) {
        char errorStr[128];
        snprintf(errorStr, sizeof(errorStr), "Update.begin() failed: %s", Update.errorString());
        setError(errorStr);
        Serial.printf("OTA ERROR: %s\n", errorStr);
        return false;
    }

    Serial.printf("OTA session started: %u bytes, %u expected chunks\n",
                 totalSize, status.expectedChunks);
    return true;
}

bool OtaWorker::queueChunk(const uint8_t* data, uint16_t length, uint32_t offset, bool isLast) {
    if (!initialized || !isActive()) {
        return false;
    }

    // Check for timeout
    if (millis() - status.lastChunkTime > OTA_CHUNK_TIMEOUT_MS) {
        setError("Chunk timeout");
        return false;
    }

    // Validate chunk size
    if (length > OTA_CHUNK_SIZE) {
        setError("Chunk too large");
        return false;
    }

    // Validate offset
    if (offset >= status.totalSize) {
        setError("Chunk offset beyond file size");
        return false;
    }

    // Create chunk structure
    OtaChunk chunk;
    chunk.offset = offset;
    chunk.totalSize = status.totalSize;
    chunk.chunkSize = length;
    chunk.isLastChunk = isLast;
    chunk.crc32 = calculateCrc32(data, length);
    memcpy(chunk.data, data, length);

    // Queue chunk for processing (non-blocking from BLE callback)
    BaseType_t result = xQueueSend(chunkQueue, &chunk, 0);
    if (result != pdPASS) {
        setError("Chunk queue full");
        return false;
    }

    status.lastChunkTime = millis();
    return true;
}

void OtaWorker::abortOta() {
    if (isActive()) {
        setError("OTA aborted by user");
        Update.abort();
    }
}

void OtaWorker::otaWorkerTask(void* parameter) {
    OtaWorker* worker = static_cast<OtaWorker*>(parameter);
    worker->processOtaQueue();
}

void OtaWorker::processOtaQueue() {
    OtaChunk chunk;

    while (true) {
        // Wait for chunk with timeout
        if (xQueueReceive(chunkQueue, &chunk, pdMS_TO_TICKS(100)) == pdPASS) {
            processChunk(chunk);
        }

        // Check for session timeout
        if (isActive() && (millis() - sessionStartTime) > OTA_SESSION_TIMEOUT_MS) {
            setError("OTA session timeout");
            Update.abort();
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void OtaWorker::processChunk(const OtaChunk& chunk) {
    // Validate chunk
    if (!validateChunk(chunk)) {
        return; // Error already set by validateChunk
    }

    // Check offset order (must be sequential for ESP32 Update library)
    if (chunk.offset != nextExpectedOffset) {
        setError("Chunk offset out of order");
        Update.abort();
        return;
    }

    // Write chunk to update partition (cast away const for Update.write)
    size_t written = Update.write(const_cast<uint8_t*>(chunk.data), chunk.chunkSize);
    if (written != chunk.chunkSize) {
        setError("Failed to write chunk");
        Update.abort();
        return;
    }

    // Update progress
    status.bytesReceived += chunk.chunkSize;
    status.chunksReceived++;
    nextExpectedOffset += chunk.chunkSize;

    Serial.printf("OTA: Chunk at offset %u processed (%u/%u bytes)\n",
                 chunk.offset, status.bytesReceived, status.totalSize);

    // Check if this is the last chunk or we've received all data
    if (chunk.isLastChunk || status.bytesReceived >= status.totalSize) {
        status.state = OTA_VALIDATING;
        Serial.println("OTA: All chunks received, validating...");

        if (Update.end(true)) {
            status.state = OTA_SUCCESS;
            Serial.println("OTA: Update successful! Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            setError("Update validation failed");
        }
    }
}

bool OtaWorker::validateChunk(const OtaChunk& chunk) {
    // Validate CRC32
    uint32_t calculatedCrc = calculateCrc32(chunk.data, chunk.chunkSize);
    if (calculatedCrc != chunk.crc32) {
        setError("Chunk CRC validation failed");
        Update.abort();
        return false;
    }

    // Validate chunk size
    if (chunk.chunkSize == 0 || chunk.chunkSize > OTA_CHUNK_SIZE) {
        setError("Invalid chunk size");
        Update.abort();
        return false;
    }

    return true;
}

uint32_t OtaWorker::calculateCrc32(const uint8_t* data, size_t length) {
    // Simple CRC32 calculation
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

void OtaWorker::setError(const char* message) {
    status.state = OTA_ERROR;
    strncpy(status.errorMessage, message, sizeof(status.errorMessage) - 1);
    status.errorMessage[sizeof(status.errorMessage) - 1] = '\0';
    Serial.printf("OTA ERROR: %s\n", message);
}

void OtaWorker::resetOtaState() {
    status.state = OTA_IDLE;
    status.totalSize = 0;
    status.bytesReceived = 0;
    status.chunksReceived = 0;
    status.expectedChunks = 0;
    status.lastChunkTime = millis();
    memset(status.errorMessage, 0, sizeof(status.errorMessage));
}