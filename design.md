# VOC Monitor System Design Document

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

## 2. User Interface Design

### 2.1 Display Hardware

- **Display**: ST7920 128x64 LCD (monochrome)
- **Library**: U8g2
- **Font**: `u8g2_font_5x8_tf` (5×8 pixels, 25 chars per line, 8 lines)
- **Interface**: Software SPI

### 2.2 Boot Screen

During startup, a boot screen is displayed showing initialization progress. This provides visual feedback during the slow WiFi connection process.

```
┌───────────────────────────────┐
│                               │
│        VOC Monitor            │  Title (6x12 font)
│                               │
│   Connecting to WiFi...       │  Status message (5x8 font)
│   ..........                  │  Progress dots (up to 10)
│                               │
└───────────────────────────────┘
        128 × 64 pixels
```

**Boot Sequence:**
1. "Initializing..." - LCD ready
2. "Init chamber sensors..." - Chamber I2C bus
3. "Init room sensors..." - Room I2C bus
4. "Connecting to WiFi..." - WiFi connection (with progress dots)
5. "WiFi connected!" or "WiFi failed, continuing..."
6. "Starting..." - Final initialization

### 2.3 Screen States (3 Cycles)

The display cycles through 3 states when the button is pressed:

1. **SCREEN_MAIN** - Main sensor data (Chamber & Room in two columns), backlight ON
2. **SCREEN_STATUS** - WiFi and system status, backlight ON
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
│Warming: 45m left              │  Sensor warmup status (or "Sensors: Ready")
└───────────────────────────────┘
        128 × 64 pixels (5x8 font)
```

**Sensor Status Display:**
- If uptime < 60 minutes: `"Warming: XXm left"` (shows minutes remaining)
- If uptime ≥ 60 minutes: `"Sensors: Ready"`

#### Page 2: Status Screen (SCREEN_STATUS)

Shows system status and uptime details. WiFi/IP removed (already shown on Page 1).

```
┌───────────────────────────────┐
│           STATUS              │  Header
├───────────────────────────────┤
│Uptime: 2d 5h 32m              │  System uptime
│Web Dashboard: ON              │  Web server status
│History: 15/30 pts             │  Data buffer status
│Avg samples: 45/60             │  Current averaging progress
│                               │
└───────────────────────────────┘
        128 × 64 pixels (5x8 font)
```

### 2.3 Screen State Machine

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
│ • Web Dashboard │                                   │
│ • History pts   │                                   │
│ • Avg samples   │                                   │
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

---

## 3. Software Design

### 3.1 Software Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       ESP8266 SOFTWARE ARCHITECTURE                     │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                      APPLICATION LAYER                            │  │
│  │                                                                   │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐    │  │
│  │  │   Display   │  │   Config    │  │   WiFi Client           │    │  │
│  │  │   Manager   │  │   Manager   │  │   (Optional)            │    │  │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        DRIVER LAYER                               │  │
│  │                                                                   │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐                 │  │
│  │  │ ENS160   │  │  AHT20   │  │    U8g2 LCD      │                 │  │
│  │  │ Driver   │  │  Driver  │  │    Display       │                 │  │
│  │  └──────────┘  └──────────┘  └──────────────────┘                 │  │
│  │                                                                   │  │
│  │  ┌──────────────────────────────────────────────────────────────┐ │  │
│  │  │                     Button Handler                           │ │  │
│  │  └──────────────────────────────────────────────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                    HAL / PLATFORM LAYER                           │  │
│  │                                                                   │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                         │  │
│  │  │   I2C    │  │   SPI    │  │  GPIO    │                         │  │
│  │  └──────────┘  └──────────┘  └──────────┘                         │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Main Program Flow

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
│  │ Handle      │                              │     │
│  │ Button      │                              │     │
│  └──────┬──────┘                              │     │
│         │                                     │     │
│         └─────────────────────────────────────┘     │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### 3.3 Project File Structure

```
voc-monitor/
├── platformio.ini              # PlatformIO configuration
├── include/
│   ├── config.h                # Pin definitions, constants
│   ├── credentials.h           # WiFi credentials (gitignored)
│   └── web_content.h           # HTML page for web dashboard
├── src/
│   └── main.cpp                # Main application
└── test/
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

## 4. Hardware Design

### 4.1 Component Selection

#### 4.1.1 ESP8266 Selection Criteria

| Requirement | ESP8266 Capability | Notes |
|-------------|-------------------|-------|
| I2C Buses | ✓ 1 Hardware + Software I2C | Required for two AHT20 sensors (fixed 0x38 address) |
| WiFi | ✓ 802.11 b/g/n | Optional remote monitoring |
| Processing Power | ✓ 80/160MHz | Sufficient for LCD + sensor processing |
| RAM | ✓ 80KB SRAM | Sufficient for 128x64 LCD |
| Flash | ✓ 4MB | Firmware + configuration storage |
| GPIO | ✓ 11 usable GPIO | Enough for LCD + 2x I2C + button |

**Recommended Board**: NodeMCU v3 (ESP-12E) or Wemos D1 Mini

### 4.2 Sensor Configuration

| Sensor | Location | Interface | I2C Address | Measurements |
|--------|----------|-----------|-------------|--------------|
| ENS160+AHT20 #1 | Chamber | I2C Bus 1 (Hardware) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |
| ENS160+AHT20 #2 | Room | I2C Bus 2 (Software) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |

**Note**: Since both AHT20 sensors have the same fixed I2C address (0x38), we use two separate I2C buses - one hardware and one software-emulated.

### 4.3 Memory Considerations

**ESP8266 Memory Budget:**
- Total SRAM: 80 KB
- WiFi stack: ~40 KB
- 128x64 LCD framebuffer: ~1 KB
- Application code + libraries: ~20 KB
- **Free heap: ~20+ KB** ✓ Sufficient for operation

Using 128x64 LCD instead of color TFT saves significant memory.

### 4.4 ESP8266 Pin Assignment

#### Pin Assignment Table

| Function | GPIO | NodeMCU Pin | Direction | Notes |
|----------|------|-------------|-----------|-------|
| **ST7920 LCD (Software SPI Mode)** |
| LCD_CLK (E) | GPIO 16 | D0 | Output | Software SPI clock |
| LCD_DATA (R/W) | GPIO 13 | D7 | Output | Software SPI data |
| LCD_CS (RS) | GPIO 0 | D3 | Output | Chip select (swapped with buzzer) |
| LCD_RST | GPIO 2 | D4 | Output | Reset (active LOW) |
| **I2C Bus 1 - Hardware (Chamber)** |
| I2C1_SDA | GPIO 4 | D2 | Bidirectional | ENS160+AHT20 |
| I2C1_SCL | GPIO 5 | D1 | Output | 100kHz |
| **I2C Bus 2 - Software (Room)** |
| I2C2_SDA | GPIO 12 | D6 | Bidirectional | ENS160+AHT20 |
| I2C2_SCL | GPIO 14 | D5 | Output | 100kHz |
| **User Input** |
| BUTTON | GPIO 1 | TX | Input | Digital interrupt (FALLING edge, internal pull-up) |
| **LCD Backlight** |
| LCD_BACKLIGHT | GPIO 3 | RX | Output | Backlight control via 47Ω resistor |
| **Audio Alert** |
| BUZZER | GPIO 15 | D8 | Output | Passive buzzer for Geiger counter effect (swapped with LCD_CS) |

**Note:** GPIO 1 (TX) is used for button input with interrupt. Serial TX is disabled when using this pin. The button uses internal pull-up and triggers on FALLING edge.

**Note:** GPIO 0 (D3) is now used for LCD_CS. GPIO 15 (D8) is used for buzzer - it has internal pull-down which doesn't affect boot.

**Note:** GPIO 3 (RX) is used for backlight control. Serial TX still works for debug output, but serial input is disabled.

### 4.5 NodeMCU Pinout Diagram

```
    NodeMCU v3 (ESP-12E)
    
    ┌─────────────────────────────────────────┐
    │  A0     ●  1                  16 ●  D0  │◄── LCD_CLK (GPIO 16)
    │         │                                │
    │  RSV    ●  2                  15 ●  D1  │◄── I2C1_SCL (GPIO 5)
    │  RSV    ●  3                  14 ●  D2  │◄── I2C1_SDA (GPIO 4)
    │  SD3    ●  4                  13 ●  D3  │◄── LCD_CS (GPIO 0)
    │  SD2    ●  5                  12 ●  D4  │◄── LCD_RST (GPIO 2)
    │  SD1    ●  6                  11 ●  3V3 │
    │  CMD    ●  7                  10 ●  GND │
    │  SD0    ●  8                   9 ●  D5  │◄── I2C2_SCL (GPIO 14)
    │  CLK    ●  9                   8 ●  D6  │◄── I2C2_SDA (GPIO 12)
    │  GND    ● 10                   7 ●  D7  │◄── LCD_DATA (GPIO 13)
    │  3V3    ● 11                   6 ●  D8  │◄── BUZZER (GPIO 15)
    │  EN     ● 12                   5 ●  RX  │◄── LCD_BACKLIGHT (GPIO 3)
    │  RST    ● 13                   4 ●  TX  │◄── BUTTON (GPIO 1)
    │  GND    ● 14                   3 ●  GND │
    │  VIN    ● 15                   2 ●  3V3 │
    ├─────────────────────────────────────────┤
    │                  │USB│                  │
    └─────────────────────────────────────────┘
```

---

## 5. Wiring & Schematics

### 5.1 ST7920 LCD Pinout (12864 Display)

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
    │ RS      │ Chip Select (CS) - GPIO 0 (D3)                         │
    │ R/W     │ Data Line - GPIO 13 (D7)                               │
    │ E       │ Clock - GPIO 16 (D0)                                   │
    │ PSB     │ Mode Select - GND = Serial, VCC = Parallel             │
    │ RST     │ Reset - GPIO 2 (D4)                                    │
    │ V0      │ Contrast - Leave floating or use 10k pot (GND=min)     │
    └─────────┴────────────────────────────────────────────────────────┘
```

### 5.2 ST7920 LCD Wiring

```
    ST7920 LCD Module               NodeMCU
    ┌─────────────┐                ┌─────┐
    │             │                │     │
    │  VCC (2) ───┼────────────────┤ VIN │  (5V from USB)
    │  GND (1) ───┼────────────────┤ GND │
    │  V0  (3) ───┼────────────────┤ NC  │  (Leave floating)
    │  RS  (4) ───┼────────────────┤ D3  │  (GPIO 0 - CS)
    │  RS  (4) ───┼────────────────┤ D3  │  (GPIO 0 - CS)
    │  R/W (5) ───┼────────────────┤ D7  │  (GPIO 13 - Data)
    │  E   (6) ───┼────────────────┤ D0  │  (GPIO 16 - Clock)
    │  PSB(15) ───┼────────────────┤ GND │  (Serial mode)
    │  RST(17) ───┼────────────────┤ D4  │  (GPIO 2 - Reset)
    │  BLA(19) ───┼──[47Ω]─────────┤ RX  │  (GPIO 3 - Backlight ctrl)
    │  BLK(20) ───┼────────────────┤ GND │
    │             │                │     │
    └─────────────┘                └─────┘
```

### 5.3 I2C Bus Connections

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

### 5.4 Button Wiring

```
    Button Wiring (using TX / GPIO 1 - Digital Interrupt)
    ═════════════════════════════════════════════════════
    
    The button is wired between TX (GPIO 1) and GND. Internal pull-up
    is enabled, and interrupt triggers on FALLING edge (button press).
    
    NodeMCU                    Button
    ┌─────┐                   ┌─────┐
    │     │                   │     │
    │ TX  ├───────────────────┤  ○──┼───┐
    │     │   (GPIO 1)        │     │   │
    │     │                   │     │   │
    │ GND ├───────────────────┤  ○──┼───┘
    │     │                   │     │
    └─────┘                   └─────┘
    
    Button States:
    - Released: GPIO 1 reads HIGH (internal pull-up)
    - Pressed: GPIO 1 reads LOW (connected to GND)
    
    Detection: Interrupt on FALLING edge with 200ms software debounce.
    
    Note: Using TX pin disables Serial output. This is acceptable since
    the device operates standalone without serial debugging in production.
```

### 5.5 Buzzer Wiring

```
    Buzzer Wiring (using GPIO 15 / D8)
    ═══════════════════════════════════
    
    A passive buzzer is connected to GPIO 15 (D8) for audio alerts.
    The buzzer produces Geiger counter-like clicks when room sensor readings
    exceed warning thresholds, and alarm beeps when room sensor has error.
    
    NodeMCU                    Passive Buzzer (2-pin)
    ┌─────┐                   ┌─────────────────┐
    │     │                   │                 │
    │ D8  ├───────────────────┤  (+)            │
    │     │   (GPIO 15)       │                 │
    │ GND ├───────────────────┤  (-)            │
    │     │                   │                 │
    └─────┘                   └─────────────────┘
    
    PASSIVE BUZZER:
    A passive buzzer is essentially a tiny speaker. It requires a PWM signal
    (from tone() function) to produce sound. The frequency of the PWM signal
    determines the pitch.
    
    WHY GPIO 15?
    GPIO 15 has an internal pull-down resistor and must be LOW at boot for
    normal boot mode. This is safe for the buzzer because:
    - At boot: GPIO 15 is LOW → buzzer silent
    - After boot: tone() generates PWM → buzzer sounds
    
    Note: GPIO 0 was previously used but caused boot issues because external
    loads on GPIO 0 can pull it LOW, triggering flash mode.
```

---

## 6. Audio Alert System

### 6.1 Geiger Counter Effect

The VOC Monitor includes an audio alert system that mimics a Geiger counter. When room sensor readings exceed warning thresholds, the buzzer produces random clicks - the higher the readings, the more frequent the clicks.

### 6.2 Alert Thresholds

The buzzer activates when **room sensor** readings exceed these thresholds:

| Metric | Warning Threshold | Maximum (fastest clicking) |
|--------|-------------------|---------------------------|
| TVOC | > 50 ppb | 2000 ppb |
| eCO2 | > 500 ppm | 5000 ppm |

**Note:** Only room sensor readings trigger the buzzer, as this indicates VOC leakage into the ambient environment (safety concern).

### 6.3 Click Frequency Calculation

The click interval is calculated based on severity:

```
Severity = max(TVOC_severity, eCO2_severity)

Where:
  TVOC_severity = (roomTVOC - 50) / (2000 - 50)  [0.0 to 1.0]
  eCO2_severity = (roomECO2 - 500) / (5000 - 500) [0.0 to 1.0]

Click Interval = 2000ms - (severity × 1950ms) ± 30% random

Result:
  - At threshold (severity = 0): ~2000ms between clicks (slow)
  - At maximum (severity = 1): ~50ms between clicks (rapid)
```

### 6.4 Buzzer Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Click Frequency | 500 Hz | Tone frequency for each click (deep sound) |
| Click Duration | 3 ms | Very short click (Geiger counter style) |
| Min Interval | 50 ms | Fastest clicking rate |
| Max Interval | 2000 ms | Slowest clicking rate |
| Randomness | ±30% | Added to interval for authentic feel |

### 6.5 Alarm Mode (Room Sensor Error)

When the room sensor is disconnected or has an error, the system enters **alarm mode**. This is a critical safety alert because a disconnected room sensor means VOC leakage cannot be detected.

**Alarm Behavior:**
- **Buzzer**: 800 Hz tone, 1 second ON / 1 second OFF (beeping pattern)
- **Display**: "ERROR" text flashes in sync with beep (inverted ↔ normal)

| Alarm Phase | Buzzer | ERROR Text Display |
|-------------|--------|-------------------|
| ON (1 sec) | 800 Hz tone | Inverted (white on black) |
| OFF (1 sec) | Silent | Normal (black on white) |

**Implementation:**
The alarm interval (1000ms) is synchronized with the sensor read interval (1000ms), so the display update naturally syncs with the alarm flashing. The alarm state is calculated once per loop iteration:
```
alarmOn = ((millis() - alarmStartTimestamp) / BUZZER_ALARM_INTERVAL) % 2 == 0
```
This ensures buzzer and display are always synchronized, and avoids redundant display redraws.

### 6.6 Behavior Summary

| Room Sensor State | Buzzer Behavior | Display |
|-------------------|-----------------|---------|
| **Disconnected/Error** | **800Hz beep (1 sec ON/OFF)** | **ERROR flashes** |
| Below thresholds | Silent | Normal readings |
| TVOC > 50 ppb OR eCO2 > 500 ppm | Slow clicking (~2 sec intervals) | Normal readings |
| TVOC ~500 ppb OR eCO2 ~1500 ppm | Medium clicking (~1 sec intervals) | Normal readings |
| TVOC ~1000 ppb OR eCO2 ~3000 ppm | Fast clicking (~0.5 sec intervals) | Normal readings |
| TVOC ≥ 2000 ppb OR eCO2 ≥ 5000 ppm | Very rapid clicking (~50ms intervals) | Normal readings |

---

## 7. Web Dashboard

### 7.1 Overview

The VOC Monitor includes a built-in web dashboard accessible via WiFi. The ESP8266 serves a responsive HTML page with real-time charts using Chart.js.

### 7.2 Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         WEB DASHBOARD ARCHITECTURE                      │
│                                                                         │
│   ┌─────────────────┐                      ┌─────────────────────────┐  │
│   │   ESP8266       │                      │   Browser (Client)      │  │
│   │                 │                      │                         │  │
│   │  ┌───────────┐  │    HTTP Request      │  ┌──────────────────┐   │  │
│   │  │ Async     │◄─┼──────────────────────┼──│ fetch('/api/...')│   │  │
│   │  │ WebServer │  │                      │  └──────────────────┘   │  │
│   │  └─────┬─────┘  │    JSON Response     │           │             │  │
│   │        │        │──────────────────────┼──►        ▼             │  │
│   │        ▼        │                      │  ┌──────────────────┐   │  │
│   │  ┌───────────┐  │                      │  │   Chart.js       │   │  │
│   │  │ History   │  │                      │  │   (CDN loaded)   │   │  │
│   │  │ Buffer    │  │                      │  └──────────────────┘   │  │
│   │  │ (30 pts)  │  │                      │                         │  │
│   │  └───────────┘  │                      │                         │  │
│   └─────────────────┘                      └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 7.3 Data Flow

```
Sensor Reading (1 Hz)
        │
        ▼
┌───────────────────┐
│ Running Average   │  ◄── Welford's incremental algorithm
│ (double precision)│      avg = avg + (new - avg) / count
└─────────┬─────────┘
          │
          │ After 60 samples (1 minute)
          ▼
┌───────────────────┐
│ History Buffer    │  ◄── Circular buffer, 30 points
│ (uint16_t × 8)    │      16 bytes per point = 480 bytes
└─────────┬─────────┘
          │
          │ Client request (/api/data)
          ▼
┌───────────────────┐
│ JSON Response     │  ◄── Oldest to newest order
│ (30 data points)  │
└───────────────────┘
```

### 7.4 Memory Usage

| Component | Size | Notes |
|-----------|------|-------|
| Running average | 65 bytes | 8 × double (64 bytes) + counter (1 byte) |
| History buffer | 480 bytes | 30 × 16 bytes per point |
| HTML page | ~2.5 KB | Stored in PROGMEM (flash), minified |
| **Total RAM** | ~550 bytes | Plus temporary stream buffers |

### 7.5 ESPAsyncWebServer

The web server uses `ESPAsyncWebServer` library for non-blocking operation:

- **Non-blocking**: Requests are handled via callbacks, not blocking the main loop
- **Efficient**: Uses LWIP async TCP callbacks and streams bytes instead of building full strings
- **Cooperative**: Requires `delay()` or `yield()` in loop for WiFi stack processing

```cpp
// No handleClient() needed in loop
void loop() {
    // ... sensor reading, display update ...
    delay(50);  // Yields to WiFi stack and async callbacks
}
```

---

## 8. API Reference

### 8.1 Endpoints

| Endpoint | Method | Response | Description |
|----------|--------|----------|-------------|
| `/` | GET | HTML | Dashboard page with Chart.js |
| `/api/current` | GET | JSON | Current running average values |
| `/api/data` | GET | JSON | Historical data (30 points) |

### 8.2 `/api/current` Response

Returns current sensor values. If sensor is disconnected, returns `"Error"`. If no data yet, returns `"N/A"`.

```json
{
  "ch_tvoc": 150,
  "ch_eco2": 450,
  "ch_temp": 28,
  "ch_rh": 55,
  "rm_tvoc": 80,
  "rm_eco2": 420,
  "rm_temp": 24,
  "rm_rh": 50,
  "samples": 45
}
```

### 8.3 `/api/data` Response

Returns historical data array (oldest to newest, up to 30 points):

```json
[
  {"ch_tvoc": 148, "ch_eco2": 448, "ch_temp": 27, "ch_rh": 54, "rm_tvoc": 78, "rm_eco2": 418, "rm_temp": 24, "rm_rh": 50},
  {"ch_tvoc": 150, "ch_eco2": 450, "ch_temp": 28, "ch_rh": 55, "rm_tvoc": 80, "rm_eco2": 420, "rm_temp": 24, "rm_rh": 50},
  ...
]
```

### 8.4 Client Refresh Rates

| Data | Refresh Interval | Reason |
|------|------------------|--------|
| Current values | 5 seconds | Near real-time display |
| History chart | 30 seconds | Data only changes every 60 seconds |

### 8.5 Chart Configuration

The web dashboard displays two charts for TVOC and eCO2 historical data.

**Chart Settings:**
- **Height**: 200px (for better readability)
- **Y-axis limits**: Capped to prevent error values (65535) from breaking the scale

| Chart | Y-axis Max | Reasoning |
|-------|------------|-----------|
| TVOC | 2,000 ppb | >1000 ppb is very poor air quality, >2000 is extreme |
| eCO2 | 4,000 ppm | >2000 ppm is poor, >5000 is dangerous |

**Error Value Handling:**
- Sensor error values (65535/65536) are filtered out and replaced with `null`
- Chart.js skips `null` values, preventing broken Y-axis scaling

---

## 9. Development Environment

### 9.1 PlatformIO Configuration

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
    mathieucarbou/ESPAsyncWebServer@^3.6.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
```

### 9.2 How to Compile

```bash
# Build
pio run

# Upload to ESP8266
pio run --target upload

# Monitor serial output
pio device monitor
```

### 9.3 Required Libraries

| Library | Purpose |
|---------|---------|
| U8g2 | 128x64 LCD driver |
| Adafruit AHTX0 | AHT20 sensor |
| SparkFun ENS160 | ENS160 sensor |
| ArduinoJson | JSON serialization for API |
| ESP8266WiFi | ESP8266 WiFi (built-in) |
| ESPAsyncWebServer | Non-blocking web server (mathieucarbou fork) |

---

## 10. Bill of Materials

### 10.1 Required Components

| Component | Model/Spec | Qty | Notes |
|-----------|------------|-----|-------|
| Microcontroller | NodeMCU v3 (ESP-12E) | 1 | Or Wemos D1 Mini |
| VOC Sensor Module | ENS160 + AHT20 Combo | 2 | I2C |
| Display | ST7920 128x64 LCD | 1 | Serial mode |
| Button | 6mm Tactile Switch | 1 | Momentary |
| Buzzer | Passive Buzzer 3-5V | 1 | For Geiger counter audio alert |
| Power Supply | 5V 1A USB | 1 | |

### 10.2 Wire Count Summary

| Connection | Wires |
|------------|-------|
| LCD (SPI) | 4 (CS, Data, CLK, RST) + 1 (Backlight) + 2 (VCC, GND) = 7 |
| I2C Bus 1 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| I2C Bus 2 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| Button | 2 (GPIO, GND) |
| **Total** | **~17 wires** |

---

## 11. Assembly & Testing

### 11.1 Assembly Checklist

- [ ] Wire I2C Bus 1 (D1/D2) to Chamber ENS160+AHT20
- [ ] Wire I2C Bus 2 (D5/D6) to Room ENS160+AHT20
- [ ] Connect LCD via Software SPI (D0, D4, D7, D8) + Backlight (RX via 47Ω)
- [ ] Wire button between TX (GPIO 1) and GND (uses internal pull-up)
- [ ] Wire buzzer between D8 (GPIO 15) and GND
- [ ] Connect 5V power via USB

### 11.2 Testing Procedure

1. **I2C Scan** - Verify 0x38 and 0x52 on both buses
2. **LCD Test** - Verify display shows text
3. **Sensor Test** - Breathe on sensors, verify readings change
4. **Button Test** - Verify screen cycling
5. **WiFi Test** - Verify connection and web dashboard
6. **Buzzer Test** - Breathe on room sensor to raise TVOC, verify clicking sound

---

## 12. References

- [ENS160 Datasheet](https://www.sciosense.com/products/environmental-sensors/ens160-digital-metal-oxide-multi-gas-sensor/)
- [AHT20 Datasheet](http://www.aosong.com/en/products-32.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [ESP8266 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp8266-technical_reference_en.pdf)
- [NodeMCU Documentation](https://nodemcu.readthedocs.io/)
- [ESPAsyncWebServer (mathieucarbou fork)](https://github.com/mathieucarbou/ESPAsyncWebServer)