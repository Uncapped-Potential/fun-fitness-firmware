# CodeCell Modular Firmware

Rebuilt firmware using Gall's Law - start with a simple working system and add complexity gradually.

## Arduino IDE Usage

**⚠️ Important**: Arduino IDE only allows one `.ino` file per directory.

### Main Firmware
- Open `codecell-firmware-modular.ino` in Arduino IDE
- This is the main production firmware

### Test Framework
- Open `test/test_compile/test_compile.ino` in Arduino IDE
- This tests compilation and module structure with mock data

## Project Structure

```
codecell-firmware-modular/
├── codecell-firmware-modular.ino    # Main sketch
├── config.h                         # Configuration constants
├── logger.h / logger.cpp            # Event logging system
├── imu_manager.h / imu_manager.cpp  # BNO085 with vendor patterns
├── ble_service.h / ble_service.cpp  # BLE GATT service
├── test/
│   └── test_compile/                # Test sketch directory
│       ├── test_compile.ino         # Test runner
│       └── [copied module files]    # Module copies for testing
└── README.md                        # This file
```

## Implementation Progress

### ✅ Phase 0: Scaffolding & Logging
- Event logging with levels (ERROR, WARN, INFO, DEBUG)
- Boot diagnostics (reset reason, partition, heap)
- Memory tracking

### ✅ Phase 1: IMU Manager
- Proper vendor patterns: single `Init()`, single `Run()` per loop
- Fusion-ready gating (wait for valid quaternions)
- Motion detection with delta calculation
- Test mode support for development

### 🚧 Phase 2: BLE Service (In Progress)
- GATT service with quaternion and config characteristics
- MTU logging and connection parameter tracking
- No heavy work in callbacks (queue-based design)

### ⏳ Phase 3: OTA Worker
- FreeRTOS queue for chunk processing
- Conservative 128-byte chunks with CRC validation
- Worker task pattern (no work in GATT callbacks)

### ⏳ Phase 4: Power Manager
- Vendor sleep/wake: `myCodeCell.Sleep(1)` and `WakeUpCheck()`
- State machine: ACTIVE → BLE_OFF → SLEEP
- Motion-based wake detection

## Key Design Decisions

1. **Vendor Pattern Compliance**
   - Single `myCodeCell.Init()` call in setup
   - Single `myCodeCell.Run()` call per loop iteration
   - Use vendor sleep/wake functions exclusively

2. **No Heavy Work in Callbacks**
   - BLE callbacks copy data to queues and return immediately
   - Worker tasks process queued data outside interrupt context

3. **Fusion-Ready Gating**
   - Never send quaternions until BNO085 fusion engine is ready
   - Wait for valid quaternion magnitude before enabling notifications

4. **Conservative OTA**
   - Start with 128-byte chunks (not 200)
   - CRC validation and rollback support
   - Timeout handling for incomplete transfers

## Testing Strategy

Each phase includes:
1. **Unit tests** with mocks (test_compile.ino)
2. **Integration test** when added to main sketch
3. **Stress testing** at completion

## Upload Instructions

1. **For main firmware**: Open `codecell-firmware-modular.ino` in Arduino IDE
2. **For testing**: Open `test/test_compile/test_compile.ino` in Arduino IDE
3. Select your ESP32-C3 board and upload
4. Monitor serial output at 115200 baud

## Expected Serial Output

```
=================================
CodeCell Modular Firmware v1.0
=================================
BOOT: Reset reason: Power-on
BOOT: Active partition: app0
BOOT: Free heap: 295000 bytes
INFO: === Phase 1 Testing ===
INFO: Device: FitChip011
INFO: Initializing IMU manager...
INFO: IMU stabilization delay (3000 ms)...
INFO: Full sensor suite initialization successful
INFO: Fusion engine ready!
INFO: IMU: quat=[x,y,z] accel=[x,y,z] motion=delta fusion=READY
```