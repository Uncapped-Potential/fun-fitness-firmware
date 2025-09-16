# OTA Implementation TODO List

## Current Status ✅
- **Working firmware**: Single BLE service with 60Hz quaternion streaming
- **Working web app**: Connects and receives data successfully
- **OTA implementation**: Complete firmware-side implementation committed to git (`ab5036c`)

## What's Done ✅

### Firmware Side (Commit: ab5036c)
- ✅ Two-service BLE architecture (Data + OTA services)
- ✅ Queue-based OTA worker with FreeRTOS task
- ✅ Proper protocol acknowledgments (OP_START → 0xa1, OP_FINISH → 0xa2)
- ✅ CRC32 validation and timeout handling
- ✅ Conservative 128-byte chunks
- ✅ Real battery status reporting (101=charging, 102=USB only)
- ✅ Service UUIDs match web app expectations

### Web App Side
- ✅ Data service connection and streaming
- ✅ Battery status detection for OTA authorization
- ✅ OTA service UUID discovery working

## What Needs to be Finished 🚧

### Web App OTA Integration

#### 1. Fix Data Transmission Issue
**Problem**: After adding OTA service, data transmission stopped working
**Root Cause**: Web app likely has connection issues with two-service architecture

**Solutions to Try**:
- [ ] **Option A**: Keep current single-service firmware, add OTA as second characteristic
- [ ] **Option B**: Debug why two-service version breaks data connection
- [ ] **Option C**: Add connection retry logic in web app

#### 2. Complete OTA Protocol Implementation
**Current State**: Web app finds OTA service but times out waiting for acknowledgments

**Required Web App Changes**:
- [ ] **Subscribe to OTA control characteristic notifications**
  ```javascript
  await otaControlChar.startNotifications();
  otaControlChar.addEventListener('characteristicvaluechanged', handleOtaAck);
  ```

- [ ] **Implement proper OP_START protocol**
  ```javascript
  // Send: [0x01][length_low][length_mid][length_high]
  const startCmd = new Uint8Array([0x01, size & 0xFF, (size >> 8) & 0xFF, (size >> 16) & 0xFF]);
  await otaControlChar.writeValue(startCmd);
  // Wait for: [0xa1][chunk_size_low][chunk_size_high]
  ```

- [ ] **Send data chunks via OTA data characteristic**
  ```javascript
  // Send chunks via: 8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0003
  await otaDataChar.writeValue(chunkData);
  ```

- [ ] **Implement OP_FINISH protocol**
  ```javascript
  // Send: [0x03]
  const finishCmd = new Uint8Array([0x03]);
  await otaControlChar.writeValue(finishCmd);
  // Wait for: [0xa2]
  ```

#### 3. Service UUIDs (Already Correct)
```javascript
// Data Service (working)
const DATA_SERVICE_UUID = "12345678-1234-1234-1234-123456789012";
const QUATERNION_CHAR_UUID = "dcba4330-dcba-4321-dcba-432123456791";

// OTA Service (implemented in firmware)
const OTA_SERVICE_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0001";
const OTA_CONTROL_CHAR_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0002";
const OTA_DATA_CHAR_UUID = "8f20d6c8-5f7d-4e7b-9b1c-0a701c3a0003";
```

## Recommended Implementation Strategy

### Phase 1: Fix Data Connection ⭐ **HIGH PRIORITY**
- [ ] Test current single-service firmware with web app
- [ ] Verify 60Hz data streaming works
- [ ] Confirm battery status (101/102) allows OTA button

### Phase 2: Add OTA as Second Characteristic (Easier)
Instead of two services, add OTA as second characteristic to existing service:
- [ ] Add OTA characteristic to data service
- [ ] Keep simpler one-service architecture
- [ ] Update web app to find OTA char in data service

### Phase 3: Complete OTA Flow
- [ ] Implement acknowledgment handling in web app
- [ ] Add proper error handling and timeouts
- [ ] Test with small firmware updates
- [ ] Add progress reporting

## File Locations

### Firmware Files
- **Current working**: `/codecell-firmware-minimal.ino` (single service)
- **OTA implementation**: `git checkout ab5036c` (two services + OTA worker)
- **OTA worker modules**: `ota_worker.h`, `ota_worker.cpp`

### Web App Changes Needed
- **Service discovery**: Add OTA service discovery
- **Characteristic handling**: Subscribe to notifications, send commands
- **Protocol implementation**: START/DATA/FINISH sequence
- **Error handling**: Timeout handling, retry logic

## Expected Timeline
- **Phase 1 (Data Fix)**: 1-2 hours
- **Phase 2 (OTA Integration)**: 4-6 hours
- **Phase 3 (Testing & Polish)**: 2-3 hours

## Success Criteria ✅
- [ ] Web app connects and shows 60Hz data streaming
- [ ] OTA button enabled when USB powered (battery 101/102)
- [ ] Firmware upload completes successfully
- [ ] Device restarts with new firmware
- [ ] No performance impact on 60Hz streaming