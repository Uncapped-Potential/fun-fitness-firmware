# BNO085 Sensor Issue - Troubleshooting Log

## Problem Evolution - Getting Worse

### Original Issue (from previous analysis)
- **Trigger**: Quick power cycles (< 15 seconds) after light sleep transitions
- **Symptom**: "Motion Sensor not found - Check Hardware" 
- **Green LED behavior**: Stayed on immediately = incomplete power reset
- **Workaround**: 15+ second wait allowed recovery

### Current Issue - Much Worse
- **New symptom**: Board won't boot properly regardless of wait time
- **Boot sequence timing**: 
  - **Normal**: Boot lights after ~1 second, green battery light immediately after
  - **Bug state**: Boot lights after 5-10 seconds, **no green battery light**
- **Firmware state**: System appears to be in deeper failure mode

## Attempted Fixes (No Effect)

### Fix #1: CPU Frequency Changes Removed ❌
**Theory**: 80MHz CPU frequency during light sleep corrupted BNO085 I2C communication
**Implementation**: Commented out all `setCpuFrequencyMhz()` calls
**Result**: No improvement - issue persists and worsened

### Fix #2: Serial Print Removal ❌  
**Theory**: Serial.print statements slowing down system performance
**Implementation**: Removed all Serial.print/printf from firmware and OTA library
**Result**: No improvement - issue persists

### Fix #3: Power Management Stabilization ❌
**Theory**: USB power detection failure during setup() caused cascade initialization failure
**Discovery**: Power readings require `myCodeCell.Run()` cycles to stabilize (PowerState=2, BatteryLevel=255 initially)
**Implementation**: Created `codecell-firmware-sleep-fixed` with power stabilization before system initialization
**Result**: No improvement - issue persists with same symptoms

## Updated Status: Power Management Not Root Cause

**Power management stabilization fix did not resolve the issue**, confirming that the problem lies elsewhere. This eliminates power reading instability as the root cause.

## Current State Analysis

### Boot Sequence Timing Issues
**Normal Boot Pattern:**
1. Power applied → ESP32 boots immediately
2. Boot lights sequence (RGB pattern) → ~1 second  
3. Green battery light activates → immediately after boot lights
4. Sensor initialization → completes successfully

**Current Bug Pattern:**
1. Power applied → **5-10 second delay before any response**
2. Boot lights sequence → finally appears after long delay
3. **Green battery light never activates** → power management failure
4. System appears stuck in initialization

### What This Tells Us About Firmware State

#### Power Management System Failure
- **Green battery light missing** = `myCodeCell.PowerStateRead()` not working
- **Delayed boot response** = ESP32 struggling with basic initialization
- **System-wide timing issues** = deeper firmware corruption than sensor-only

#### Possible Root Causes

1. **Flash Memory Corruption**
   - Repeated power cycling may have corrupted firmware in flash
   - Boot delay suggests ESP32 struggling to read/execute code
   - Power management initialization failing completely

2. **Hardware Power Rail Issues**  
   - 3.3V rail instability affecting entire system, not just BNO085
   - Power sequencing problems affecting ESP32 boot timing
   - Battery management chip communication failure

3. **I2C Bus Complete Failure**
   - Both BNO085 (sensor) and power management using I2C
   - Bus wedged in a state that affects multiple devices
   - ESP32 I2C peripheral corrupted/stuck

4. **RTC Memory/Configuration Corruption**
   - Boot count/state variables corrupted: `RTC_DATA_ATTR bool sensorInitialized`
   - System trying to restore invalid power state
   - Configuration flags causing initialization loop

### Critical Diagnostic Questions

1. **Does the device respond to simple firmware?**
   - Flash a minimal "blink LED" program
   - If this fails → hardware issue
   - If this works → complex firmware issue

2. **Is I2C bus functional?**
   - Test basic I2C scanner code
   - Check if any I2C devices respond (BNO085 @ 0x4A, power management)

3. **Power management chip communication?**
   - `myCodeCell.PowerStateRead()` timing out during boot
   - Battery voltage reading failing
   - USB detection not working

## Recommended Next Steps

### Immediate Diagnostics
1. **Flash minimal test firmware** (LED blink only)
2. **I2C bus scanner** to check device communication
3. **Oscilloscope on I2C lines** (if available) during boot
4. **Measure power rail voltages** during problematic boot

### Recovery Attempts
1. **Complete firmware re-flash** (erase + write)
2. **Factory reset** of ESP32 configuration
3. **Hardware power cycle** (disconnect battery + USB for extended time)

## Hypothesis Update

**Original**: CPU frequency changes during light sleep corrupt BNO085 sensor communication
**Current**: System-wide initialization failure affecting power management, I2C bus, and basic ESP32 operation

The issue has **escalated beyond sensor-only problems** to fundamental system boot failures.