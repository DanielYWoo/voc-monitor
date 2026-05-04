# VoC Monitor System Design Document

## 1. Overview

This document provides the detailed technical design for an industrial-grade environmental monitoring system. While optimized for 3D printer enclosures (Voron Trident and Bedslinger) for safe ABS printing, this is a **standalone project** that can be used for any VOC monitoring scenario including workshops, labs, or industrial environments.

### 1.1 Design Goals

- **Standalone Operation**: Independent system, not tightly coupled to Klipper/Raspberry Pi
- **Safety**: Real-time VOC monitoring with visual feedback
- **Reliability**: Dual-zone monitoring (Chamber + Room)
- **Flexibility**: Optional WiFi integration for remote monitoring
- **24/7 Operation**: Independent power system for continuous monitoring
- **Usability**: Local LCD display for immediate feedback

### 1.2 System Architecture

```
┌───────────────────────────────────────┐       ┌─────────────────────────────────┐
│           CHAMBER (Enclosure)         │       │          ROOM (Ambient)         │
│                                       │       │                                 │
│  ┌─────────────────────────────────┐  │       │  ┌─────────────────────────────┐│
│  │       ENS160+AHT20              │  │       │  │       ENS160+AHT20          ││
│  │       (I2C Combo)               │  │       │  │       (I2C Combo)           ││
│  │                                 │  │       │  │                             ││
│  │  • TVOC (ppb)                   │  │       │  │  • TVOC (ppb)               ││
│  │  • eCO2 (ppm)                   │  │       │  │  • eCO2 (ppm)               ││
│  │  • Temperature (°C)             │  │       │  │  • Temperature (°C)         ││
│  │  • Humidity (%)                 │  │       │  │  • Humidity (%)             ││
│  └────────────┬────────────────────┘  │       │  └────────────┬────────────────┘│
│               │                       │       │               │                 │
└───────────────┼───────────────────────┘       └───────────────┼─────────────────┘
                │                                               │
                │                                               │
┌───────────────▼───────────────────────────────────────────────▼─────────────────┐
│                              ESP8266 CONTROLLER                                  │
│                                                                                  │
│   ┌──────────────────────────────────────────────────────────────────────────┐  │
│   │                           SENSOR INTERFACES                               │  │
│   │                                                                           │  │
│   │   ┌───────────────────┐  ┌───────────────────┐                           │  │
│   │   │    I2C Bus 1      │  │    I2C Bus 2      │                           │  │
│   │   │    (Chamber)      │  │     (Room)        │                           │  │
│   │   │   Hardware I2C    │  │   Software I2C    │                           │  │
│   │   │   GPIO 4/5        │  │   GPIO 12/14      │                           │  │
│   │   │        ▲          │  │        ▲          │                           │  │
│   │   │        │          │  │        │          │                           │  │
│   │   │  ENS160+AHT20     │  │  ENS160+AHT20     │                           │  │
│   │   └────────┼──────────┘  └────────┼──────────┘                           │  │
│   │            │                      │                                       │  │
│   │            └──────────────────────┘                                       │  │
│   │                        │                                                  │  │
│   │               ┌────────▼────────┐                                         │  │
│   │               │  Data Processor │                                         │  │
│   │               └────────┬────────┘                                         │  │
│   └────────────────────────┼──────────────────────────────────────────────────┘  │
│                            │                                                     │
│   ┌────────────────────────┼──────────────────────────────────────────────────┐  │
│   │                    OUTPUT LAYER                                            │  │
│   │                        │                                                   │  │
│   │      ┌──────────────────┐      ┌───────────────┐                          │  │
│   │      │  ST7920 128x64   │      │  WiFi Client  │                          │  │
│   │      │  LCD (Serial)    │      │  (Optional)   │                          │  │
│   │      └──────────────────┘      └───────────────┘                          │  │
│   └───────────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Hardware Design

### 2.1 Component Selection

#### 2.1.1 ESP8266 Selection Criteria

| Requirement | ESP8266 Capability | Notes |
|-------------|-------------------|-------|
| I2C Buses | ✓ 1 Hardware + Software I2C | Required for two AHT20 sensors (fixed 0x38 address) |
| WiFi | ✓ 802.11 b/g/n | Optional remote monitoring |
| Processing Power | ✓ 80/160MHz | Sufficient for LCD + sensor processing |
| RAM | ✓ 80KB SRAM | Sufficient for 128x64 LCD |
| Flash | ✓ 4MB | Firmware + configuration storage |
| GPIO | ✓ 11 usable GPIO | Enough for LCD + 2x I2C + button |

**Recommended Board**: NodeMCU v3 (ESP-12E) or Wemos D1 Mini

### 2.2 Sensor Configuration

| Sensor | Location | Interface | I2C Address | Measurements |
|--------|----------|-----------|-------------|--------------|
| ENS160+AHT20 #1 | Chamber | I2C Bus 1 (Hardware) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |
| ENS160+AHT20 #2 | Room | I2C Bus 2 (Software) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |

**Note**: Since both AHT20 sensors have the same fixed I2C address (0x38), we use two separate I2C buses - one hardware and one software-emulated.

### 2.3 Memory Considerations

**ESP8266 Memory Budget:**
- Total SRAM: 80 KB
- WiFi stack: ~40 KB
- 128x64 LCD framebuffer: ~1 KB
- Application code + libraries: ~20 KB
- **Free heap: ~20+ KB** ✓ Sufficient for operation

Using 128x64 LCD instead of color TFT saves significant memory.

### 2.4 ESP8266 Pin Assignment

#### Pin Assignment Table

| Function | GPIO | NodeMCU Pin | Direction | Notes |
|----------|------|-------------|-----------|-------|
| **ST7920 LCD (Software SPI Mode)** |
| LCD_CLK (E) | GPIO 16 | D0 | Output | Software SPI clock |
| LCD_DATA (R/W) | GPIO 13 | D7 | Output | Software SPI data |
| LCD_CS (RS) | GPIO 15 | D8 | Output | Chip select |
| LCD_RST | GPIO 2 | D4 | Output | Reset (active LOW) |
| **I2C Bus 1 - Hardware (Chamber)** |
| I2C1_SDA | GPIO 4 | D2 | Bidirectional | ENS160+AHT20 |
| I2C1_SCL | GPIO 5 | D1 | Output | 100kHz |
| **I2C Bus 2 - Software (Room)** |
| I2C2_SDA | GPIO 12 | D6 | Bidirectional | ENS160+AHT20 |
| I2C2_SCL | GPIO 14 | D5 | Output | 100kHz |
| **User Input** |
| BUTTON | GPIO 0 | D3 | Input | Internal pull-up, FLASH button |
| **LCD Backlight** |
| LCD_BACKLIGHT | GPIO 3 | RX | Output | Backlight control via 47Ω resistor |

**Note:** GPIO 0 is the FLASH button on most NodeMCU boards. It has an internal pull-up and can be used as a regular button input when not in programming mode.

**Note:** GPIO 3 (RX) is used for backlight control. Serial TX still works for debug output, but serial input is disabled.

### 2.5 NodeMCU Pinout Diagram

```
    NodeMCU v3 (ESP-12E)
    
    ┌─────────────────────────────────────────┐
    │  A0     ●  1                  16 ●  D0  │◄── LCD_CLK (GPIO 16)
    │  RSV    ●  2                  15 ●  D1  │◄── I2C1_SCL (GPIO 5)
    │  RSV    ●  3                  14 ●  D2  │◄── I2C1_SDA (GPIO 4)
    │  SD3    ●  4                  13 ●  D3  │◄── BUTTON (GPIO 0)
    │  SD2    ●  5                  12 ●  D4  │◄── LCD_RST (GPIO 2)
    │  SD1    ●  6                  11 ●  3V3 │
    │  CMD    ●  7                  10 ●  GND │
    │  SD0    ●  8                   9 ●  D5  │◄── I2C2_SCL (GPIO 14)
    │  CLK    ●  9                   8 ●  D6  │◄── I2C2_SDA (GPIO 12)
    │  GND    ● 10                   7 ●  D7  │◄── LCD_DATA (GPIO 13)
    │  3V3    ● 11                   6 ●  D8  │◄── LCD_CS (GPIO 15)
    │  EN     ● 12                   5 ●  RX  │◄── LCD_BACKLIGHT (GPIO 3)
    │  RST    ● 13                   4 ●  TX  │
    │  GND    ● 14                   3 ●  GND │
    │  VIN    ● 15                   2 ●  3V3 │
    ├─────────────────────────────────────────┤
    │                  │USB│                  │
    └─────────────────────────────────────────┘
```

### 2.6 ST7920 LCD Pinout (12864 Display)

The ST7920 is a common 128x64 LCD controller. We use **serial mode** to minimize GPIO usage.

```
                    ST7920 128x64 LCD Module (12864)
                   ┌─────────────────────────────────┐
                   │  Pin   Name    Function         │
                   │  ───   ────    ────────         │
                   │   1    GND     Ground           │
                   │   2    VCC     Power (5V)       │
                   │   3    V0      Contrast         │◄── Leave floating or use 10k pot
                   │   4    RS      Register Select  │◄── GPIO 15 (D8) - CS
                   │   5    R/W     Read/Write       │◄── GPIO 13 (D7) - Data
                   │   6    E       Enable           │◄── GPIO 16 (D0) - Clock
                   │  7-14  DB0-7   Data Bus (NC)    │    (Not used in serial mode)
                   │  15    PSB     Bus Select       │◄── GND (Serial mode)
                   │  16    NC      Not Connected    │
                   │  17    RST     Reset            │◄── GPIO 2 (D4) - Reset
                   │  18    VOUT    LCD Drive (NC)   │
                   │  19    BLA     Backlight +      │◄── 47Ω resistor -> RX (GPIO 3)
                   │  20    BLK     Backlight -      │◄── GND
                   └─────────────────────────────────┘

    Software SPI Mode Pin Functions:
    ┌─────────┬────────────────────────────────────────────────────────┐
    │ LCD Pin │ Function                                               │
    ├─────────┼────────────────────────────────────────────────────────┤
    │ RS      │ Chip Select (CS) - GPIO 15 (D8)                        │
    │ R/W     │ Data Line - GPIO 13 (D7)                               │
    │ E       │ Clock - GPIO 16 (D0)                                   │
    │ PSB     │ Mode Select - GND = Serial, VCC = Parallel             │
    │ RST     │ Reset - GPIO 2 (D4)                                    │
    │ V0      │ Contrast - Leave floating or use 10k pot (GND=min)     │
    └─────────┴────────────────────────────────────────────────────────┘
```

### 2.7 Circuit Schematics

#### 2.7.1 ST7920 LCD Wiring

```
    ST7920 LCD Module               NodeMCU
    ┌─────────────┐                ┌─────┐
    │             │                │     │
    │  VCC (2) ───┼────────────────┤ VIN │  (5V from USB)
    │  GND (1) ───┼────────────────┤ GND │
    │  V0  (3) ───┼────────────────┤ NC  │  (Leave floating)
    │  RS  (4) ───┼────────────────┤ D8  │  (GPIO 15 - CS)
    │  R/W (5) ───┼────────────────┤ D7  │  (GPIO 13 - Data)
    │  E   (6) ───┼────────────────┤ D0  │  (GPIO 16 - Clock)
    │  PSB(15) ───┼────────────────┤ GND │  (Serial mode)
    │  RST(17) ───┼────────────────┤ D4  │  (GPIO 2 - Reset)
    │  BLA(19) ───┼──[47Ω]─────────┤ RX  │  (GPIO 3 - Backlight ctrl)
    │  BLK(20) ───┼────────────────┤ GND │
    │             │                │     │
    └─────────────┘                └─────┘
```

#### 2.7.2 I2C Bus Connections

```
    I2C BUS 1 - Hardware (Chamber)         I2C BUS 2 - Software (Room)
    ══════════════════════════════         ═══════════════════════════
    
    NodeMCU        ENS160+AHT20            NodeMCU        ENS160+AHT20
    ┌─────┐       ┌─────────────┐          ┌─────┐       ┌─────────────┐
    │ D2  ├───────┤ SDA         │          │ D6  ├───────┤ SDA         │
    │ D1  ├───────┤ SCL         │          │ D5  ├───────┤ SCL         │
    │ 3V3 ├───────┤ VCC         │          │ 3V3 ├───────┤ VCC         │
    │ GND ├───────┤ GND         │          │ GND ├───────┤ GND         │
    └─────┘       └─────────────┘          └─────┘       └─────────────┘
    
    WHY TWO I2C BUSES?
    The AHT20 has a FIXED I2C address of 0x38 that cannot be changed.
    To use two AHT20 sensors, we need two separate I2C buses.
    ESP8266 has 1 hardware I2C, so we use software I2C for the second bus.
```

#### 2.7.3 Button Wiring

```
    Button Wiring (using GPIO 0 / D3)
    ═════════════════════════════════
    
    The button is wired between GPIO 0 and GND. GPIO 0 has an internal
    pull-up resistor, so no external resistor is needed.
    
    NodeMCU                    Button
    ┌─────┐                   ┌─────┐
    │     │                   │     │
    │ D3  ├───────────────────┤  ○──┼───┐
    │     │   (GPIO 0)        │     │   │
    │ GND ├───────────────────┤  ○──┼───┘
    │     │                   │     │
    └─────┘                   └─────┘
    
    Button States:
    - Released: GPIO 0 reads HIGH (pulled up internally)
    - Pressed: GPIO 0 reads LOW (connected to GND)
    
    Note: GPIO 0 is also the FLASH button on NodeMCU. During normal
    operation, it works as a regular button. Only during boot does
    it affect programming mode (hold LOW during reset = flash mode).
```

---

## 3. Display Design

### 3.1 Display Hardware

- **Display**: ST7920 128x64 LCD (monochrome)
- **Library**: U8g2
- **Font**: `u8g2_font_5x8_tf` (5×8 pixels, 25 chars per line, 8 lines)
- **Interface**: Software SPI

### 3.2 Screen States (3 Cycles)

The display cycles through 3 states when the button is pressed:

1. **SCREEN_MAIN** - Main sensor data (Chamber & Room in two columns), backlight ON
2. **SCREEN_STATUS** - WiFi and Klipper push status, backlight ON
3. **SCREEN_MAIN_DARK** - Main sensor data, backlight OFF (power saving)

#### Page 1: Main Screen (SCREEN_MAIN)

Two-column layout with table borders showing Chamber and Room sensor data side by side.

```
┌───────────────┬───────────────┐
│   CHAMBER     │     ROOM      │  Header row
├───────────────┼───────────────┤
│TVOC:    150ppb│TVOC:     50ppb│  TVOC values (right-aligned)
│eCO2:    800ppm│eCO2:    450ppm│  eCO2 values (right-aligned)
│T:35C RH:30%   │T:25C RH:50%   │  Temp & humidity
├───────────────┴───────────────┤
│WiFi: Connected                │  WiFi status
│IP: 192.168.1.100              │  IP address
└───────────────────────────────┘
        128 × 64 pixels (5x8 font)
```

#### Page 2: Status Screen (SCREEN_STATUS)

Shows WiFi and Klipper push status details.

```
┌───────────────────────────────┐
│           STATUS              │  Header
├───────────────────────────────┤
│WiFi: Connected                │  WiFi status
│IP: 192.168.1.100              │  IP address
│Klipper Push: ON               │  Push enabled flag
│Host: 192.168.1.50:7125        │  Klipper endpoint
│Last Push: OK                  │  Last push result
└───────────────────────────────┘
        128 × 64 pixels (5x8 font)
```

### 3.3 Screen State Machine

```
┌─────────────────┐
│   MAIN SCREEN   │◄─────────────────────────────────┐
│  (Backlight ON) │                                   │
│                 │                                   │
│ • Chamber TVOC  │                                   │
│ • Chamber eCO2  │                                   │
│ • Chamber T/RH  │                                   │
│ • Room TVOC     │                                   │
│ • Room eCO2     │                                   │
│ • Room T/RH     │                                   │
│ • WiFi status   │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         ▼                                            │
┌─────────────────┐                                   │
│  STATUS SCREEN  │                                   │
│  (Backlight ON) │                                   │
│                 │                                   │
│ • WiFi status   │                                   │
│ • IP address    │                                   │
│ • Klipper Push  │                                   │
│ • Klipper Host  │                                   │
│ • Last push     │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         ▼                                            │
┌─────────────────┐                                   │
│   MAIN SCREEN   │                                   │
│ (Backlight OFF) │                                   │
│                 │                                   │
│  Same as above  │                                   │
│  but backlight  │                                   │
│  is turned off  │                                   │
│  for power      │                                   │
│  saving         │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         └────────────────────────────────────────────┘
```

### 3.4 Sensor Calibration Requirements

| Sensor | User Calibration | Frequency | Method |
|--------|------------------|-----------|--------|
| **ENS160** | ⚠️ Auto | Every power-on | Self-calibrates over ~1 hour, no user action |
| **AHT20** | ❌ Not needed | N/A | Factory calibrated, ±0.3°C / ±2% RH accuracy |

**ENS160 Auto-Calibration:**
- Uses built-in baseline algorithm
- Takes ~1 hour after power-on to stabilize
- Learns environment over 24 hours for best accuracy
- No user intervention required

---

## 4. Software Design

### 4.1 Software Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       ESP8266 SOFTWARE ARCHITECTURE                      │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                      APPLICATION LAYER                             │  │
│  │                                                                    │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐    │  │
│  │  │   Display   │  │   Config    │  │   WiFi Client           │    │  │
│  │  │   Manager   │  │   Manager   │  │   (Optional)            │    │  │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        DRIVER LAYER                                │  │
│  │                                                                    │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐                 │  │
│  │  │ ENS160   │  │  AHT20   │  │    U8g2 LCD      │                 │  │
│  │  │ Driver   │  │  Driver  │  │    Display       │                 │  │
│  │  └──────────┘  └──────────┘  └──────────────────┘                 │  │
│  │                                                                    │  │
│  │  ┌──────────────────────────────────────────────────────────────┐ │  │
│  │  │                     Button Handler                            │ │  │
│  │  └──────────────────────────────────────────────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                    HAL / PLATFORM LAYER                            │  │
│  │                                                                    │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                         │  │
│  │  │   I2C    │  │   SPI    │  │  GPIO    │                         │  │
│  │  └──────────┘  └──────────┘  └──────────┘                         │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Project File Structure

```
voc-monitor/
├── platformio.ini              # PlatformIO configuration
├── include/
│   ├── config.h                # Pin definitions, constants
│   └── credentials.h           # WiFi credentials (gitignored)
├── src/
│   └── main.cpp                # Main application
└── test/
```

### 4.3 Main Program Flow

```
┌─────────────────┐
│     START       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Initialize     │
│  - I2C Buses    │
│  - SPI (LCD)    │
│  - WiFi (opt)   │
│  - Sensors      │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────────────────┐
│                    MAIN LOOP                         │
│                                                      │
│  ┌─────────────┐                                    │
│  │ Read Sensors│◄─────────────────────────────┐     │
│  │ (1 second)  │                              │     │
│  └──────┬──────┘                              │     │
│         │                                     │     │
│         ▼                                     │     │
│  ┌─────────────┐                              │     │
│  │ Update      │                              │     │
│  │ Display     │                              │     │
│  └──────┬──────┘                              │     │
│         │                                     │     │
│         ▼                                     │     │
│  ┌─────────────┐                              │     │
│  │ WiFi Upload │                              │     │
│  │ (if enabled)│                              │     │
│  └──────┬──────┘                              │     │
│         │                                     │     │
│         ▼                                     │     │
│  ┌─────────────┐                              │     │
│  │ Handle      │                              │     │
│  │ Button      │                              │     │
│  └──────┬──────┘                              │     │
│         │                                     │     │
│         └─────────────────────────────────────┘     │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## 5. Development Environment

### 5.1 PlatformIO Configuration

```ini
; platformio.ini

[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_speed = 921600

lib_deps = 
    olikraus/U8g2@^2.35.0
    adafruit/Adafruit AHTX0@^2.0.3
    sparkfun/SparkFun Indoor Air Quality Sensor - ENS160@^1.0.0
    bblanchon/ArduinoJson@^6.21.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
```

### 5.2 How to Compile

```bash
# Build
pio run

# Upload to ESP8266
pio run --target upload

# Monitor serial output
pio device monitor
```

### 5.3 Required Libraries

| Library | Purpose |
|---------|---------|
| U8g2 | 128x64 LCD driver |
| Adafruit AHTX0 | AHT20 sensor |
| SparkFun ENS160 | ENS160 sensor |
| ArduinoJson | JSON for WiFi |
| ESP8266WiFi | ESP8266 WiFi (built-in) |

---

## 6. Bill of Materials

### 6.1 Required Components

| Component | Model/Spec | Qty | Notes |
|-----------|------------|-----|-------|
| Microcontroller | NodeMCU v3 (ESP-12E) | 1 | Or Wemos D1 Mini |
| VOC Sensor Module | ENS160 + AHT20 Combo | 2 | I2C |
| Display | ST7920 128x64 LCD | 1 | Serial mode |
| Button | 6mm Tactile Switch | 1 | Momentary |
| Power Supply | 5V 1A USB | 1 | |

### 6.2 Wire Count Summary

| Connection | Wires |
|------------|-------|
| LCD (SPI) | 4 (CS, Data, CLK, RST) + 1 (Backlight) + 2 (VCC, GND) = 7 |
| I2C Bus 1 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| I2C Bus 2 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| Button | 2 (GPIO, GND) |
| **Total** | **~17 wires** |

---

## 7. Assembly & Testing

### 7.1 Assembly Checklist

- [ ] Wire I2C Bus 1 (D1/D2) to Chamber ENS160+AHT20
- [ ] Wire I2C Bus 2 (D5/D6) to Room ENS160+AHT20
- [ ] Connect LCD via Software SPI (D0, D4, D7, D8) + Backlight (RX via 47Ω)
- [ ] Wire button between D3 (GPIO 0) and GND
- [ ] Connect 5V power via USB

### 7.2 Testing Procedure

1. **I2C Scan** - Verify 0x38 and 0x52 on both buses
2. **LCD Test** - Verify display shows text
3. **Sensor Test** - Breathe on sensors, verify readings change
4. **Button Test** - Verify screen cycling
5. **WiFi Test** - Verify connection and data push

---

## 8. References

- [ENS160 Datasheet](https://www.sciosense.com/products/environmental-sensors/ens160-digital-metal-oxide-multi-gas-sensor/)
- [AHT20 Datasheet](http://www.aosong.com/en/products-32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [ESP8266 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp8266-technical_reference_en.pdf)
- [NodeMCU Documentation](https://nodemcu.readthedocs.io/)