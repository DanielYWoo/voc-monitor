# VOC Monitor for 3D Printer Enclosures

A standalone VOC (Volatile Organic Compounds) monitoring system for 3D printer enclosures, built on ESP32 with a 128x64 ST7920 LCD.

## Features

- **Dual-zone monitoring**: Chamber (inside enclosure) and Room (ambient)
- **ENS160+AHT20 sensors**: TVOC, eCO2, Temperature, Humidity
- **MQ135 backup sensor**: Toluene-equivalent PPM estimation
- **128x64 ST7920 LCD**: Real-time data visualization (12864 display)
- **Button-controlled screens**: Main → Status → Off
- **Optional WiFi**: Push data to Klipper/Moonraker

## Hardware Requirements

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| Microcontroller | ESP32-WROOM-32E DevKit | 1 | 38-pin |
| VOC Sensor | ENS160+AHT20 Combo | 2 | I2C |
| Gas Sensor | MQ135 Module | 1 | Analog |
| Display | ST7920 128x64 LCD (12864) | 1 | Serial mode |
| Button | 6mm Tactile Switch | 1 | Momentary |
| Resistors | 10kΩ + 20kΩ | 1 each | Voltage divider |
| Resistor | 100Ω | 1 | LCD backlight |

## Wiring

### ST7920 128x64 LCD (12864) - Serial Mode

The ST7920 is a common 128x64 LCD used in 3D printers (like the Ender 3 stock display). It runs on **5V** but accepts 3.3V logic inputs from ESP32.

**Important:** Set PSB pin to GND to enable serial (SPI) mode.

```
    ST7920 LCD                      ESP32 DevKit
    ┌───────────┐                   ┌───────────┐
    │           │                   │           │
    │  VCC  ────┼───────────────────┤ 5V (VIN)  │
    │           │                   │           │
    │  GND  ────┼───────────────────┤ GND       │
    │           │                   │           │
    │  RS   ────┼───────────────────┤ GPIO 5    │  (CS - Chip Select)
    │           │                   │           │
    │  R/W  ────┼───────────────────┤ GPIO 23   │  (MOSI - Data)
    │           │                   │           │
    │  E    ────┼───────────────────┤ GPIO 18   │  (SCK - Clock)
    │           │                   │           │
    │  PSB  ────┼───────────────────┤ GND       │  (Serial mode select)
    │           │                   │           │
    │  RST  ────┼───────────────────┤ GPIO 4    │  (Reset, or tie to VCC)
    │           │                   │           │
    │  BLA  ────┼──[100Ω]───────────┤ 5V        │  (Backlight +)
    │           │                   │           │
    │  BLK  ────┼───────────────────┤ GND       │  (Backlight -)
    │           │                   │           │
    └───────────┘                   └───────────┘
```

| LCD Pin | ESP32 GPIO | Function |
|---------|------------|----------|
| VCC | 5V (VIN) | Power (5V required!) |
| GND | GND | Ground |
| RS | GPIO 5 | Chip Select |
| R/W | GPIO 23 | Data (MOSI) |
| E | GPIO 18 | Clock (SCK) |
| PSB | GND | Serial mode (tie to GND) |
| RST | GPIO 4 | Reset (optional, can tie to VCC) |
| BLA | 5V via 100Ω | Backlight anode |
| BLK | GND | Backlight cathode |

### I2C Sensors
| Sensor | SDA | SCL |
|--------|-----|-----|
| Chamber (ENS160+AHT20) | GPIO 21 | GPIO 22 |
| Room (ENS160+AHT20) | GPIO 16 | GPIO 17 |

### Other
| Component | GPIO |
|-----------|------|
| MQ135 (via voltage divider) | GPIO 32 |
| Button | GPIO 26 (to GND) |

## Quick Start

### Prerequisites

1. **Install PlatformIO** (recommended) or Arduino IDE
   ```bash
   # Install PlatformIO CLI (if not using VS Code extension)
   pip install platformio
   ```

2. **Install USB-to-Serial Driver** (if needed)
   - For CH340/CH341 chips: [CH341 Driver](https://www.wch.cn/downloads/CH341SER_MAC_ZIP.html)
   - For CP2102 chips: [CP210x Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

### Build and Flash

#### Step 1: Clone/Download the Project
```bash
cd VOC-monitor
```

#### Step 2: Connect ESP32 to Computer
- Use a USB data cable (not charge-only)
- The ESP32 should appear as a serial port:
  - **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
  - **Windows**: `COM3`, `COM4`, etc.
  - **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`

#### Step 3: Build the Firmware
```bash
# Using PlatformIO CLI
pio run

# Or using VS Code: Click the checkmark (✓) in the bottom toolbar
```

#### Step 4: Flash to ESP32
```bash
# Using PlatformIO CLI
pio run --target upload

# Or using VS Code: Click the arrow (→) in the bottom toolbar
```

**If upload fails:**
1. Hold the **BOOT** button on ESP32 while clicking upload
2. Release BOOT after "Connecting..." appears
3. Try lowering upload speed in `platformio.ini`:
   ```ini
   upload_speed = 460800  ; or 115200
   ```

#### Step 5: Monitor Serial Output
```bash
pio device monitor

# Or using VS Code: Click the plug icon in the bottom toolbar
```

You should see:
```
=== VOC Monitor UI Demo ===

Display initialized
Free heap: 280000 bytes

Press button (GPIO 26) to cycle screens:
  OFF -> MAIN -> STATUS -> OFF
```

### Troubleshooting

| Problem | Solution |
|---------|----------|
| "No serial port found" | Install USB driver, try different cable |
| "Failed to connect" | Hold BOOT button during upload |
| Display blank | Check wiring, verify **5V** power, PSB→GND |
| Garbled display | Check PSB is tied to GND (serial mode) |
| Very dim display | Add 100Ω resistor to backlight, check BLA/BLK |
| Button not working | Check GPIO 26 wiring to GND |

## WiFi Setup (Optional)

WiFi is disabled in the UI demo. To enable:

1. Create `include/credentials.h`:
   ```cpp
   #ifndef CREDENTIALS_H
   #define CREDENTIALS_H
   
   #define WIFI_SSID "your_wifi_ssid"
   #define WIFI_PASSWORD "your_wifi_password"
   
   // Klipper/Moonraker host (optional)
   #define KLIPPER_HOST "192.168.1.100"
   #define KLIPPER_PORT 7125
   
   #endif
   ```

2. Add to `.gitignore`:
   ```
   include/credentials.h
   ```

3. Uncomment WiFi code in `main.cpp` (future implementation)

## Project Structure

```
VOC-monitor/
├── platformio.ini          # Build configuration
├── include/
│   ├── config.h            # Pin definitions, constants
│   └── credentials.h       # WiFi credentials (gitignored)
├── src/
│   └── main.cpp            # Main application (UI demo)
├── design.md               # Detailed design document
├── spec.md                 # Original specification
└── README.md               # This file
```

## Display Layout

### Main Screen
```
┌─────────────────────────────────┐
│   CHAMBER     │     ROOM        │
│TVOC ▓▓1245ppb░│TVOC ▓52ppb░░░░░░│
│eCO2 ▓▓2892ppm░│eCO2 ▓421ppm░░░░░│
│T:58°C  RH:23% │T:24°C  RH:45%   │
├─────────────────────────────────┤
│ Room Backup                     │
│ Toluene:2.5ppm T:24°C RH:50%    │
└─────────────────────────────────┘
```

### Status Screen
```
┌─────────────────────────────────┐
│ SYSTEM STATUS                   │
├─────────────────────────────────┤
│ WiFi: Connected                 │
│ IP: 192.168.1.105               │
│                                 │
│ Klipper Push: Enabled           │
│ Host: 192.168.1.100             │
│ Last: OK                        │
│ Free Mem: 142384                │
└─────────────────────────────────┘
```

## Button Operation

Press the button (GPIO 26) to cycle through screens:
- **OFF** → **MAIN** → **STATUS** → **OFF** → ...

## License

MIT License - See design.md for full documentation.
