#include "ble_service.h"

BleService::BleService()
    : initialized(false)
    , notificationsEnabled(true)
    , oldConnected(false)
    , lastNotify(0)
    , currentDataRate(ACTIVE_DATA_RATE)
    , removeGravity(false)
    , sleepTimeoutSec(BLE_INACTIVITY_TIMEOUT / 1000)
#if !TEST_MODE_ENABLED
    , pServer(nullptr)
    , pMainService(nullptr)
    , pQuaternionChar(nullptr)
    , pConfigChar(nullptr)
    , pAdvertising(nullptr)
#endif
{
    connectionInfo = {false, 0, 0, 0, 0};
}

bool BleService::init() {
    LOG_INFO("BLE service init() - Phase 2 not implemented yet");
    initialized = true;
    return true;
}

void BleService::update() {
    // Placeholder for Phase 2
}

void BleService::setNotificationsEnabled(bool enabled) {
    notificationsEnabled = enabled;
    LOG_INFO("BLE notifications %s", enabled ? "ENABLED" : "DISABLED");
}

bool BleService::sendImuData(const ImuManager::SensorData& data) {
    if (!notificationsEnabled) return false;

    LOG_DEBUG("BLE: Would send IMU data quat=[%.3f,%.3f,%.3f]", data.qx, data.qy, data.qz);
    return true;
}

void BleService::setGravityRemoval(bool enabled) {
    removeGravity = enabled;
}

void BleService::setDataRate(uint8_t rateHz) {
    currentDataRate = rateHz;
}

void BleService::setSleepTimeout(uint8_t timeoutSec) {
    sleepTimeoutSec = timeoutSec;
}