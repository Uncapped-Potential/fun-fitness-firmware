# Fun Fitness Firmware

ESP32 firmware for the Fun Fitness wireless game controller with motion tracking and battery monitoring.

## Features

- **9-axis IMU** - Real-time motion tracking (accelerometer, gyroscope, rotation)
- **BLE Communication** - 50Hz data streaming to web application
- **Battery Monitoring** - Real-time battery level reporting and LED indicators
- **Auto-reconnect** - Automatic BLE advertising restart on disconnect
- **Power Management** - Sleep mode with motion-based wake (coming soon)

## Hardware

- **Board**: CodeCell ESP32
- **Sensors**: Built-in 9-axis IMU
- **Communication**: Bluetooth Low Energy (BLE)
- **Power**: Battery with USB charging

## Data Format

The firmware streams IMU and battery data in CSV format at 50Hz:

```
R:45.123,P:-12.456,Y:180.789,AX:0.123,AY:-0.456,AZ:9.789,GX:1.234,GY:-2.345,GZ:0.567,B:85
```

- **R**: Roll (degrees)
- **P**: Pitch (degrees)
- **Y**: Yaw (degrees)
- **AX/AY/AZ**: Accelerometer X/Y/Z (m/s²)
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
3. Open `codecell-firmware.ino` in Arduino IDE

### Board Configuration

1. In Arduino IDE, go to **Tools > Board > Boards Manager**
2. Search for "ESP32" and install "esp32 by Espressif Systems"
3. Select **Tools > Board > ESP32 Arduino > ESP32C3 Dev Module**

### Required Libraries

Install via **Tools > Manage Libraries**:
- **CodeCell** - For CodeCell hardware support
- **ESP32** by Espressif - Core ESP32 support (BLE is included)

### Upload

1. Connect CodeCell via USB
2. Select correct port in **Tools > Port**
3. Click Upload button or press Cmd+U

## Configuration

- **Data rate**: 50Hz (adjustable in code)
- **BLE Service UUID**: `12345678-1234-1234-1234-123456789012`
- **BLE Characteristic UUID**: `dcba4330-dcba-4321-dcba-432123456791`

## Troubleshooting

- **Upload fails**: Ensure you have selected ESP32C3 Dev Module and correct port
- **Libraries not found**: Install via Library Manager, not manual download
- **macOS permissions**: May need to allow Arduino IDE in Security & Privacy settings

## Issues

See [GitHub Issues](https://github.com/Uncapped-Potential/fun-fitness-firmware/issues) for bug reports and feature requests.

## Related Projects

- [fun-fitness-web](https://github.com/Uncapped-Potential/fun-fitness-web) - Web application that receives controller data

## License

[Add your license here]