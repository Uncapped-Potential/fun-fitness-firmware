#ifndef OTA_WORKER_H
#define OTA_WORKER_H

#include "config.h"
#include <Arduino.h>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class OtaWorker {
public:
    enum OtaState {
        OTA_IDLE,
        OTA_RECEIVING,
        OTA_VALIDATING,
        OTA_APPLYING,
        OTA_SUCCESS,
        OTA_ERROR
    };

    struct OtaChunk {
        uint32_t sequence;
        uint32_t totalSize;
        uint16_t chunkSize;
        uint32_t crc32;
        uint8_t data[OTA_CHUNK_SIZE];
        bool isLastChunk;
    };

    struct OtaStatus {
        OtaState state;
        uint32_t totalSize;
        uint32_t bytesReceived;
        uint32_t chunksReceived;
        uint32_t expectedChunks;
        uint32_t lastChunkTime;
        char errorMessage[64];
    };

    OtaWorker();
    ~OtaWorker();

    // Initialize OTA worker (call once in setup)
    bool init();

    // Start OTA session (call when beginning update)
    bool startOta(uint32_t totalSize);

    // Queue chunk for processing (call from BLE callback)
    bool queueChunk(const uint8_t* data, uint16_t length, uint32_t sequence, bool isLast);

    // Get current status
    const OtaStatus& getStatus() const { return status; }

    // Abort current OTA session
    void abortOta();

    // Check if OTA is in progress
    bool isActive() const { return status.state != OTA_IDLE && status.state != OTA_SUCCESS && status.state != OTA_ERROR; }

private:
    static void otaWorkerTask(void* parameter);
    void processOtaQueue();
    void processChunk(const OtaChunk& chunk);
    bool validateChunk(const OtaChunk& chunk);
    uint32_t calculateCrc32(const uint8_t* data, size_t length);
    void setError(const char* message);
    void resetOtaState();

    TaskHandle_t workerTaskHandle;
    QueueHandle_t chunkQueue;
    OtaStatus status;
    bool initialized;

    // OTA session state
    uint32_t nextExpectedSequence;
    uint32_t sessionStartTime;

    // Buffer for chunk assembly (if needed)
    uint8_t* assemblyBuffer;
    size_t assemblyBufferSize;
};

#endif // OTA_WORKER_H