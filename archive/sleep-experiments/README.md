# Sleep Mode Experiments Archive

## Overview
This directory contains experimental implementations of sleep/wake functionality that were attempted but did not work as expected. These are preserved for reference and future debugging.

## Experiments Included

### 1. serial-test/
- **Purpose**: Basic serial communication test to debug why Serial output wasn't working on "Sara" chip
- **Result**: No serial output at any baud rate (115200, 9600, etc.)
- **Conclusion**: Possible hardware issue with specific CodeCell unit

### 2. proximity-sleep/
- **Purpose**: Official CodeCell sleep example using proximity sensor for wake detection
- **Source**: Based on CodeCell documentation example
- **Result**: LED control conflicts, couldn't verify sleep/wake behavior
- **Issue**: Underlying firmware appears to control LED, preventing debug feedback

### 3. BNO085 Motion State Implementation (in main firmware - reverted)
- **Purpose**: Use BNO085's built-in motion state detection for sleep/wake
- **Features**:
  - Motion states: On Table (1), Stationary (2), Stable (3), Motion (4)
  - 8-second sleep timeout for testing (90 seconds for production)
  - ESP32 light sleep with 1-second wake intervals
- **Result**: 
  - ✅ Successfully enters sleep after timeout
  - ❌ Never wakes on motion
  - ❌ BLE doesn't restart after wake
  - ❌ No serial debug output

## Key Findings

1. **Hardware Issues**: "Sara" chip appears to have non-functional serial, possibly damaged
2. **API Uncertainty**: CodeCell's `Sleep()` and `WakeUpCheck()` behavior unclear
3. **Motion Detection Works**: BNO085 motion state detection functions correctly when awake
4. **Sleep Entry Works**: Device successfully enters sleep mode
5. **Wake Mechanism Broken**: Once asleep, device doesn't properly wake on motion

## Technical Details

### Attempted Approaches:
1. **ESP32 Light Sleep**: Preserves RAM but device doesn't wake properly
2. **Delay-based Sleep**: Removed ESP32 sleep, used delays - still no wake
3. **Proximity Wake**: Official example, but couldn't verify due to LED conflicts

### Code Structure:
```cpp
// Key functions attempted:
checkSleepWakeState() - Monitor motion state and trigger sleep
enterLightSleep() - ESP32 light sleep with timer wake  
Motion_StateRead() - BNO085 built-in motion classification
```

## Recommendations for Future Attempts

1. **Test with Different Hardware**: Rule out "Sara" chip damage
2. **Contact Microbot Support**: Clarify Sleep/WakeUpCheck API behavior
3. **Start Simple**: Timer-only sleep without motion detection
4. **External Wake Source**: Consider button or interrupt-based wake
5. **Debug Without Serial**: Use LED patterns or BLE data for debugging

## Related Issues
- GitHub Issue #6: ESP32 Sleep Mode Implementation Causes Multiple Critical Failures
- GitHub Issue #1: Implement sleep mode with motion-based wake

## Note
The main firmware has been reverted to the working version without sleep functionality. All core features (BLE streaming, battery monitoring, auto-reconnect) work perfectly without sleep mode.