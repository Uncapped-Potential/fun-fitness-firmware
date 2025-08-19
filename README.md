# Fun Fitness Firmware

ESP32 firmware for the Fun Fitness wireless game controller with motion tracking and battery monitoring.

## Features

- **9-axis IMU** - Real-time motion tracking (accelerometer, gyroscope, rotation, magnetometer)
- **BLE Communication** - 24Hz data streaming to web application (optimized for battery life)
- **Binary Protocol** - Compact 19-byte format (77% smaller than text)
- **Battery Monitoring** - Real-time battery level reporting and LED indicators
- **Auto-reconnect** - Automatic BLE advertising restart on disconnect
- **Power Management** - Sleep mode with motion-based wake (coming soon)

## Hardware

- **Board**: CodeCell ESP32-C3
- **Sensors**: Built-in 9-axis IMU with magnetometer
- **Communication**: Bluetooth Low Energy (BLE)
- **Power**: Battery with USB charging

## Bill of Materials (BOM)

### Core Components

1. **CodeCell Light + Motion (170mAh battery)**
   - Product: [CodeCell from Microbots.io](https://microbots.io/products/codecell)
   - Variant: Light + Motion / 170mAh 20C LiPo / Unsoldered
   - Specifications:
     - ESP32-C3 32-bit RISC-V (160 MHz)
     - BNO085 9-axis IMU (accelerometer, gyroscope, magnetometer)
     - VCNL4040 light/proximity sensor
     - 170mAh LiPo battery included
     - Dimensions: 18.5 x 18.5 x 9.4 mm
   - Included: 4x M1.2 self-tapping screws (~5mm) for mounting

2. **Power Switch**
   - Product: [SPDT Mini Slide Switch](https://www.amazon.com/dp/B0DN69PJ43)
   - Specifications:
     - Model: SS12D00G
     - Size: 8.7 x 3.7 x 11.7mm (L x W x H)
     - Pin Pitch: 0.1" / 2.54mm
     - Type: SPDT on-off, 3 pins
     - Mounting: Vertical through-hole, panel mount
     - Sliding knob length: 3mm
   - Quantity needed: 1

### Connectors and Cables

3. **JST 1.25mm Connectors**
   - Product: [JST 1.25mm 2-Pin Male/Female Cables](https://www.amazon.com/dp/B013JRWCBU)
   - Use: Battery connection
   - Quantity needed: 1 male-female pair

4. **USB-C Cable**
   - Product: [USB-C to USB-C Cable](https://www.amazon.com/dp/B0CGNK4LTT) (or any USB-C cable)
   - Use: Programming and charging
   - Quantity needed: 1

### Mounting Hardware

5. **M1.2 Nuts**
   - Product: [Stainless Steel Screw Set](https://www.amazon.com/dp/B0DBHNJLTD)
   - Specifications: M1.2 x 0.25 pitch
   - Quantity needed: 2

6. **M1.2 Bolts**
   - Product: [Laptop Screw Set](https://www.amazon.com/dp/B0D8PMBPB7)
   - Specifications:
     - 4x M1.2 x 5mm bolts
     - 2x M1.2 x 6mm bolts
   - Quantity needed: 6 total

7. **Mounting Adhesive**
   - Product: [Command Strips Velcro](https://www.amazon.com/dp/B0B3SR5M71) or any adhesive velcro
   - Use: Mounting controller to surfaces
   - Quantity needed: As required

### 3D Printed Enclosure

8. **Enclosure Components** (see `/enclosure` directory)
   - **Files**: Up V7 Body, Up V7 lid, Up V7 switch cradle
   - **Material**: **FIRE RETARDANT (PC-FR) REQUIRED** for production use
   - **Quantity**: 1 of each component
   - **Note**: Standard materials acceptable for internal testing only

### Safety Equipment

9. **LiPo Safety Bag**
   - Product: [PAWBOSE Fireproof LiPo Battery Bag](https://www.amazon.com/PAWBOSE-Fireproof-185x75x60mm-Explosionproof-Protection/dp/B0F6LP63HD/)
   - Specifications: 185x75x60mm fireproof/explosionproof case
   - Use: Safe storage and charging of controller
   - **HIGHLY RECOMMENDED** for any LiPo battery use

### Assembly Notes

- The CodeCell comes with 4x M1.2 self-tapping screws for direct mounting
- JST connectors are for connecting the power switch inline with the battery
- M1.2 nuts and bolts provide additional mounting options
- **SAFETY**: Always use fire retardant materials for enclosure in production
- **SAFETY**: Store and charge controller in LiPo safety bag
- Total assembled weight: ~8g (3.4g board + 4.6g battery)

## Quick Order BOM

For easy procurement, here's a table with links and quantities needed for **one FitChip assembly**:

| Component | Link | Qty |
|-----------|------|-----|
| CodeCell Light+Motion (170mAh) | [Microbots](https://microbots.io/products/codecell) | 1 |
| SPDT Slide Switch | [Amazon B0DN69PJ43](https://www.amazon.com/dp/B0DN69PJ43) | 1 |
| JST 1.25mm Male/Female Cables | [Amazon B013JRWCBU](https://www.amazon.com/dp/B013JRWCBU) | 1 pair |
| M1.2 Screw/Nut Set | [Amazon B0DBHNJLTD](https://www.amazon.com/dp/B0DBHNJLTD) | 1 set |
| Laptop Screws (M1.2) | [Amazon B0D8PMBPB7](https://www.amazon.com/dp/B0D8PMBPB7) | 1 set |
| USB-C Cable | [Amazon B0CGNK4LTT](https://www.amazon.com/dp/B0CGNK4LTT) | 1 |
| Adhesive Velcro | [Amazon B0B3SR5M71](https://www.amazon.com/dp/B0B3SR5M71) | As needed |
| **Safety Equipment** | | |
| LiPo Safety Bag | [Amazon B0F6LP63HD](https://www.amazon.com/PAWBOSE-Fireproof-185x75x60mm-Explosionproof-Protection/dp/B0F6LP63HD/) | 1 |

## Required Tools & Materials

| Tool/Material | Purpose | Source |
|---------------|---------|---------|
| **3D Printer** | Print enclosure components | Required |
| **Fire Retardant Filament (PC-FR)** | Enclosure printing | [Bambu Lab PC-FR](https://us.store.bambulab.com/products/pc-fr) |
| **Soldering Iron** | Wire connections | Required |
| **Heat Gun** | Heat shrink application | Required |
| **Heat Shrink Tubing** | Wire insulation | Required |
| **Tweezers** | Component handling | Required |

**3D Print Files:** See `/enclosure` directory - **fire retardant material required for production**

## Data Protocol

The firmware supports two streaming formats for backward compatibility:

### Binary Protocol (Default - 19 bytes)

Compact binary format for efficient data transmission:

```
Byte Layout:
[0-1]   Roll (int16_t)      - Range: ±327°,     Scale: 0.01°
[2-3]   Pitch (int16_t)     - Range: ±327°,     Scale: 0.01°
[4-5]   Yaw (int16_t)       - Range: ±327°,     Scale: 0.01°
[6-7]   Accel X (int16_t)   - Range: ±65.5g,    Scale: 0.002g
[8-9]   Accel Y (int16_t)   - Range: ±65.5g,    Scale: 0.002g
[10-11] Accel Z (int16_t)   - Range: ±65.5g,    Scale: 0.002g
[12-13] Gyro X (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s
[14-15] Gyro Y (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s
[16-17] Gyro Z (int16_t)    - Range: ±3276°/s,  Scale: 0.1°/s
[18]    Battery (uint8_t)   - Range: 0-255,     Direct mapping
```

### Text Protocol (Legacy - ~85 bytes)

CSV format for older web app versions:

```
R:45.123,P:-12.456,Y:180.789,AX:0.123,AY:-0.456,AZ:9.789,GX:1.234,GY:-2.345,GZ:0.567,B:85
```

- **R**: Roll (degrees)
- **P**: Pitch (degrees)
- **Y**: Yaw (degrees)
- **AX/AY/AZ**: Accelerometer X/Y/Z (g-force)
- **GX/GY/GZ**: Gyroscope X/Y/Z (deg/s)
- **B**: Battery level (1-100%, 101=charging, 102=USB power)

## LED Indicators

- **Green** (≥80%): Good battery
- **Yellow** (50-79%): Medium battery
- **Orange** (20-49%): Low battery
- **Red blink** (<20%): Critical battery
- **Green breathing**: Charging

## Development Setup

### Prerequisites

- **Arduino IDE** - Download from [arduino.cc](https://www.arduino.cc/en/software)
- **macOS** - May require Xcode and newest macOS version
- **USB Driver** - For ESP32 serial communication

### Installation

1. Download and install Arduino IDE
2. Clone this repository:
   ```bash
   git clone https://github.com/Uncapped-Potential/fun-fitness-firmware.git
   cd fun-fitness-firmware
   ```
3. Open `codecell-firmware/codecell-firmware.ino` in Arduino IDE (must have nested folder and file with same name, quirk of arduino)

### Guide [Video](https://drive.google.com/file/d/13qVB6rGZdJrtGnBQS_VcSYYFrTN_345D/view?usp=vids_web) 

### Board Configuration

1. In Arduino IDE, go to **Tools > Board > Boards Manager**
2. Search for "ESP32" and install "esp32 by Espressif Systems"
3. Select **Tools > Board > ESP32 Arduino > ESP32C3 Dev Module**

### Required Libraries

Install via **Tools > Manage Libraries**:
- **CodeCell** - For CodeCell hardware support

**Note**: ESP32 core support is installed via Boards Manager (see Board Configuration above), not Library Manager. BLE support is included with the ESP32 core.

### Upload

1. Connect CodeCell via USB
2. Select correct port in **Tools > Port**
3. Click Upload button or press Cmd+U

## Configuration

- **Data rate**: 24Hz (optimized from 50Hz for battery life)
- **Protocol**: Binary by default (19 bytes per packet)
- **BLE Service UUID**: `12345678-1234-1234-1234-123456789012`
- **BLE Characteristic UUID**: `dcba4330-dcba-4321-dcba-432123456791`

## Performance & Optimization

### Binary Protocol Benefits

- **77% bandwidth reduction**: 19 bytes vs ~85 bytes per packet
- **Lower power consumption**: Less data to transmit over BLE
- **Higher precision**: Fixed-point encoding preserves accuracy
- **Future-proof**: Supports high g-force gaming movements (±65.5g)

### Protocol Selection

The web application automatically detects the protocol format using content-based analysis:
- Binary data: 19-byte packets with specific structure
- Text data: Contains pattern "R:...P:...Y:..."

No firmware changes needed - the web app handles both formats seamlessly.

## Troubleshooting

- **Upload fails**: Ensure you have selected ESP32C3 Dev Module and correct port
- **Libraries not found**: Install via Library Manager, not manual download
- **macOS permissions**: May need to allow Arduino IDE in Security & Privacy settings
- **High g-force readings**: Binary protocol supports up to ±65.5g for vigorous gaming
- **Battery drain**: Reduced polling to 24Hz and binary protocol optimize battery life
- **Device name shows old name in Chrome**: Chrome Web Bluetooth caches device names persistently. After updating firmware device name, Chrome may still show the old name. Solutions:
  - Use a different browser (Safari, Firefox) 
  - Restart Chrome completely
  - Use Chrome Incognito mode
  - Connect with a different device/computer
  - Note: This is a browser caching issue, not a firmware problem
- **Upload errors "chip stopped responding"**: If you get upload failures after potential pin shorting:
  1. Try uploading again (may work on retry)
  2. If persistent, erase flash: `esptool --port /dev/cu.usbmodem1101 erase_flash`
  3. Then upload firmware normally through Arduino IDE
  4. Device should recover and show "New connection established" in serial monitor

## Known Issues

- **Sleep Mode**: Motion-based wake not yet implemented (Issue #6)
- **IMU Overflow**: Data may overflow after ~10 minutes of continuous use
- **LED Conflicts**: Some LED library conflicts may occur with certain CodeCell versions

See [GitHub Issues](https://github.com/Uncapped-Potential/fun-fitness-firmware/issues) for bug reports and feature requests.

## Changelog

### v2.0.0 (Current)
- Added binary protocol (77% size reduction)
- Reduced polling rate to 24Hz for battery optimization
- Increased acceleration range to ±65.5g for gaming
- Added magnetometer support
- Improved battery level reporting at 24Hz

### v1.0.0
- Initial release with text-based CSV protocol
- 50Hz IMU streaming
- Battery monitoring with LED indicators
- Auto-reconnect support

## Related Projects

- [fun-fitness-web](https://github.com/Uncapped-Potential/fun-fitness-web) - Web application that receives controller data

## License

[Add your license here]
