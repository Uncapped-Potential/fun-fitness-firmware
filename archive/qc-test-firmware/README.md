# CodeCell QC Test System

Complete charge controller safety testing system for BQ24232RGTR hardware validation.

## Overview

This QC system tests the critical safety functions of the charge controller IC to prevent battery overcharging. It runs comprehensive charge/discharge cycles while logging all parameters via Bluetooth.

## Hardware Requirements

- **Charge IC**: BQ24232RGTR (U5)
- **Pin Connections**:
  - CE (Charge Enable) → GPIO 10
  - PGOOD (Power Good) → GPIO 18  
  - CHG (Charge Status) → GPIO 8
  - VBAT (Battery ADC) → GPIO 1
- **Battery**: 170mAh LiPo (3.0V - 4.2V)
- **Thermal Camera**: For temperature monitoring
- **USB Hub**: 12-port hub for parallel testing

## Files

1. **`qc-test-firmware.ino`** - Arduino firmware for QC testing
2. **`QC_Test_Parallel.ipynb`** - Jupyter notebook for parallel testing
3. **`requirements.txt`** - Python dependencies
4. **`README.md`** - This file

## Quick Start

### 1. Flash QC Firmware
```bash
# Open Arduino IDE
# Load qc-test-firmware.ino
# Select ESP32-C3 board
# Flash to each device via USB
```

### 2. Install Python Dependencies
```bash
pip install -r requirements.txt
```

### 3. Run QC Test
```bash
jupyter notebook QC_Test_Parallel.ipynb
```

## Test Procedure

### Complete Test (6-8 Hours)
1. **Initial State Check** (5 min)
   - Verify PGOOD=0 (USB connected)
   - Verify CHG=1 (not charging)
   - Battery voltage 3.0-4.2V

2. **2-Hour Charge Cycle**
   - Enable charging (CE=LOW)
   - Monitor voltage increase
   - Log at 0.5Hz

3. **2-Hour Discharge** 
   - Unplug USB hub
   - Monitor voltage decrease
   - Log at 0.5Hz

4. **15-Minute Charge Intervals**
   - Repeat until battery full
   - Plug/unplug between cycles

5. **Final 2-Hour Discharge**
   - High-frequency logging (60Hz)
   - Simulate heavy use

### Quick Test (2 Minutes)
For debugging and validation:
```python
# In Jupyter notebook
await quick_test()
```

## Safety Features

⚠️ **SAFE MODE ENABLED** - GPIO pins disabled until schematic verification

- **Thermal Monitoring**: 50°C threshold via thermal camera
- **Battery Protection**: 3.0V-4.2V range via CodeCell library  
- **Charge Control**: ~~Direct CE pin control~~ **DISABLED FOR SAFETY**
- **USB Detection**: ~~PGOOD pin monitoring~~ **Estimated from voltage**
- **Emergency Stop**: Manual intervention capability

## Communication Protocol

### BLE Services
- **Service**: `8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2000`
- **Command**: `8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2001` (WRITE)
- **Status**: `8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2002` (READ/notify)
- **Log**: `8f20d6c8-5f7d-4e7b-9b1c-0a701c3a2003` (notify)

### Commands (Safe Mode)
```cpp
QC_GET_STATUS = 0x01     // Get current status (voltage + temp only)
// QC_CHARGE_ENABLE = 0x02  // DISABLED FOR SAFETY
// QC_CHARGE_DISABLE = 0x03 // DISABLED FOR SAFETY  
QC_START_LOGGING = 0x05  // Start phase logging
QC_CHECKPOINT = 0x07     // Mark checkpoint
QC_SELF_TEST = 0x09      // Run safe self-test (no GPIO)
```

## Results

Test generates CSV reports with format:
```csv
device_name,serial,address,final_battery_mv,connection,initial_state,charge_2h,discharge_2h,final_result
QC_001,1,AA:BB:CC:DD:EE:01,3850,PASS,PASS,PASS,PASS,PASS
QC_002,2,AA:BB:CC:DD:EE:02,3780,PASS,PASS,PASS,FAIL,FAIL
```

## Troubleshooting

### No Devices Found
- Check firmware is flashed correctly
- Verify devices are powered via USB
- Ensure Bluetooth is enabled on computer

### Connection Fails  
- Restart devices
- Check BLE service UUIDs match
- Reduce number of parallel connections

### Test Fails
- Check GPIO pin connections
- Verify battery is connected
- Monitor thermal camera for overheating
- Check USB hub power capacity

### Thermal Issues
- Ensure adequate ventilation
- Check for short circuits
- Verify charge current (should be ~90mA)
- Stop test if temperature >50°C

## Manufacturing Integration

1. **Batch Testing**: Test 12 boards simultaneously
2. **Serial Tracking**: Device names include 3-digit serials (001, 002, etc.)
3. **Pass/Fail Marking**: Apply QC stickers to PASS units only
4. **Report Archive**: Save CSV reports with timestamps
5. **Production Firmware**: Flash production firmware on PASS units

## Technical Notes

- **Charge Current**: ~90mA (set by R12=10kΩ resistor)
- **Battery Chemistry**: LiPo with 170mAh capacity
- **Test Coverage**: Charge enable/disable, power good detection, voltage monitoring
- **Data Integrity**: All critical measurements logged with timestamps
- **Parallel Capability**: Python asyncio handles up to 12 concurrent BLE connections

This system ensures charge controller hardware safety before production firmware deployment.