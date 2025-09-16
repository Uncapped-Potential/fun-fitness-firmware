# BNO085 Sensor Issue - Complete Troubleshooting Report

## Problem Summary

**Original Issue**: CodeCell firmware getting stuck during sensor initialization, particularly after quick power cycles (< 15 seconds), showing "Motion Sensor not found - Check Hardware" error and causing system hangs.

**Root Cause Identified**: BNO085 sensor state corruption due to insufficient capacitor discharge time combined with Arduino IDE compilation overhead when using separate .cpp/.h library files.

## Symptom Evolution

### Phase 1: Original Symptoms
- **Trigger**: Quick power cycles after light sleep transitions  
- **Symptoms**: "Motion Sensor not found - Check Hardware", system hangs for 5-10 seconds
- **Green LED**: Missing (indicating power management failure)
- **Workaround**: Required 15+ second power-off to recover

### Phase 2: Problem Escalation  
- Same triggers now caused complete system failure
- 5-10 second boot delays before ANY response
- Green battery LED never appeared
- Even 15+ second waits stopped working reliably

## Investigation Process

### Systematic Testing Approach (Gall's Law)
Created isolated test suite to identify failure point:

1. **Phase 1 - I2C Bus Test** ✅ PASSED
   - I2C scan found BNO085 at 0x4A
   - Bus communication working

2. **Phase 2 - Power Management Test** ✅ PASSED (after fixes)
   - Initially showed PowerState=2, BatteryLevel=255 (uninitialized)
   - **Discovery**: Power readings require `myCodeCell.Run()` cycles to stabilize
   - Fixed with stabilization delays

3. **Phase 3A - Single Sensor Test** ✅ PASSED
   - Accelerometer-only initialization worked
   - Valid readings: [0.707, 7.406, -6.379] m/s²

4. **Phase 3B - Dual Sensor Test** ✅ PASSED (after serial fix)
   - Initially missing serial output (Serial.begin() not called)
   - After fix: Both accelerometer and gyroscope worked

5. **Phase 3C - Full Sensor Suite Test** ✅ REVEALED KEY INSIGHT
   - **Critical Discovery**: Sensors come online gradually over 4-6 seconds
   - Accelerometer/Gyro: Immediate
   - Magnetometer/Quaternion: ~2 seconds  
   - Gravity Vector: ~4-6 seconds

## Root Cause Analysis

### Primary Issue: BNO085 State Corruption
- **Hardware Constraint**: BNO085 reset pin tied to 3.3V (cannot force hardware reset)
- **Capacitor Discharge**: Quick power cycles don't allow full discharge
- **State Persistence**: Corrupted internal state survives soft resets

### Secondary Issue: Arduino IDE Compilation Overhead
- **OTA Library Decomposition**: Moving OTA code to separate .cpp/.h files
- **Compilation Impact**: Arduino IDE treats .cpp files as libraries with different compilation/linking
- **Performance Penalty**: Slower boot times, memory layout changes, initialization order issues

## Failed Solutions

### ❌ CPU Frequency Changes
- **Theory**: 80MHz during light sleep corrupted I2C communication
- **Action**: Removed all `setCpuFrequencyMhz()` calls
- **Result**: No improvement

### ❌ Serial Print Removal  
- **Theory**: Serial output causing timing issues
- **Action**: Removed all Serial.print statements
- **Result**: No improvement

### ❌ Power Management Stabilization
- **Theory**: USB power detection failure during setup()
- **Action**: Added power state stabilization logic
- **Result**: Fixed power readings but didn't resolve sensor issue

### ❌ BNO085 Software Reset (Direct Access)
- **Theory**: Reset sensor before initialization
- **Action**: Attempted to access global BNO085 object directly
- **Result**: Memory access fault - object not accessible

### ❌ Watchdog Timer Protection
- **Theory**: Prevent infinite hangs during initialization
- **Action**: Added ESP32 watchdog with 8-second timeout
- **Result**: Successfully detected hangs but BLE initialization also started hanging

## Successful Solutions

### ✅ Commit Reversion Analysis
- **Method**: Systematically reverted commits to find working version
- **Key Finding**: Commit 9c1bdb8 worked reliably
- **Breaking Commit**: 466c91a "Decompose OTA functionality" introduced issues

### ✅ OTA Library Impact Identification
- **Discovery**: Removing OTA .cpp/.h files restored fast boot times
- **Root Cause**: Arduino IDE compilation overhead when processing separate library files
- **Improvement**: Power cycle recovery time reduced from 15+ seconds to 3-4 seconds

### ✅ Enhanced Retry Logic
- **Implementation**: Added power-on reset detection and longer stabilization delays
- **Result**: More reliable sensor initialization with shorter recovery times

## Final Solution

### Code Changes Made:
1. **Removed OTA library dependencies** temporarily for testing
2. **Added power-on reset detection** with extra stabilization time
3. **Increased retry delays** from 2 seconds to 3 seconds between sensor init attempts
4. **Enhanced error reporting** with detailed reset reason logging

### Performance Improvements:
- **Boot time**: Significantly faster without OTA library overhead
- **Recovery time**: Reduced from 15+ seconds to 3-4 seconds power-off
- **Reliability**: More consistent sensor initialization

## Technical Insights

### Arduino IDE Compilation Behavior
- **.ino files**: Compiled as single unit, faster linking
- **.cpp/.h files**: Compiled as separate libraries, additional overhead
- **Impact**: Memory layout differences, initialization timing changes

### BNO085 Sensor Characteristics  
- **Initialization timing**: Fusion outputs need 4-6 seconds to stabilize
- **State persistence**: Internal state survives soft resets when reset pin tied to power
- **Recovery method**: Requires sufficient capacitor discharge time (3-4 seconds minimum)

### Power Management Discovery
- **CodeCell library**: Power readings require `Run()` cycles to initialize internal state
- **Initial values**: PowerState=2, BatteryLevel=255 indicate uninitialized state
- **Stabilization**: Needs multiple Run() cycles before returning valid readings

## Recommendations

### Immediate Actions:
1. **Test final firmware version** with OTA dependencies removed and enhanced retry logic
2. **Validate 3-4 second power cycle requirement** is consistently sufficient
3. **Consider user documentation** about power cycle timing if needed

### Future Improvements:
1. **OTA Integration**: If OTA functionality needed, inline code in .ino file rather than separate library
2. **Hardware Modification**: Consider adding GPIO-controlled power to BNO085 for clean resets
3. **Sensor Validation**: Implement staggered sensor validation allowing fusion outputs time to stabilize

### Production Considerations:
1. **Power Cycle Timing**: 3-4 seconds is acceptable for normal user scenarios
2. **Error Recovery**: System now handles sensor failures gracefully with automatic retry
3. **Diagnostic Logging**: Enhanced error reporting helps with field troubleshooting

## Files Modified:
- `codecell-firmware-sleep/codecell-firmware-sleep.ino`: Enhanced with power-on detection and retry logic
- `BNO085_TROUBLESHOOTING_LOG.md`: Original investigation log
- Test suite: `test-phase1-i2c/`, `test-phase2-power-debug/`, `test-phase3a-single-sensor/`, etc.

## Key Learning:
**The combination of hardware constraints (reset pin tied to power) and software compilation overhead (separate library files) created a perfect storm for sensor state corruption. The solution required both identifying the compilation impact and implementing appropriate recovery timing.**