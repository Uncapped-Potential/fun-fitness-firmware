#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "config.h"
#include "logger.h"
#include "imu_manager.h"

#if !TEST_MODE_ENABLED
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

class BleService {
public:
    struct ConnectionInfo {
        bool connected;
        uint16_t mtu;
        uint16_t interval;
        uint16_t latency;
        uint16_t timeout;
    };

    BleService();

    // Initialize BLE service
    bool init();

    // Update service - call in main loop
    void update();

    // Start/stop notifications based on power state
    void setNotificationsEnabled(bool enabled);

    // Send IMU data if notifications are active
    bool sendImuData(const ImuManager::SensorData& data);

    // Get connection status
    const ConnectionInfo& getConnectionInfo() const { return connectionInfo; }
    bool isConnected() const { return connectionInfo.connected; }

    // Configuration interface
    void setGravityRemoval(bool enabled);
    void setDataRate(uint8_t rateHz);
    void setSleepTimeout(uint8_t timeoutSec);

private:
#if !TEST_MODE_ENABLED
    BLEServer* pServer;
    BLEService* pMainService;
    BLECharacteristic* pQuaternionChar;
    BLECharacteristic* pConfigChar;
    BLEAdvertising* pAdvertising;
#endif

    ConnectionInfo connectionInfo;
    bool initialized;
    bool notificationsEnabled;
    bool oldConnected;

    // Rate limiting
    unsigned long lastNotify;
    uint8_t currentDataRate;

    // Configuration state
    bool removeGravity;
    uint8_t sleepTimeoutSec;

    void startAdvertising();
    void handleReconnection();
    void logConnectionParams();
    void packImuData(const ImuManager::SensorData& data, uint8_t* buffer);

    friend class BleServerCallbacks;
    friend class ConfigCharCallbacks;
};

#if !TEST_MODE_ENABLED
// BLE callback classes (friends of BleService)
class BleServerCallbacks : public BLEServerCallbacks {
public:
    BleServerCallbacks(BleService* service) : bleService(service) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    BleService* bleService;
};

class ConfigCharCallbacks : public BLECharacteristicCallbacks {
public:
    ConfigCharCallbacks(BleService* service) : bleService(service) {}
    void onWrite(BLECharacteristic* pCharacteristic) override;

private:
    BleService* bleService;
};
#endif

#endif // BLE_SERVICE_H