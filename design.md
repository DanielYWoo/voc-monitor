# VoC Monitor System Design Document

## 1. Overview

This document provides the detailed technical design for an industrial-grade environmental monitoring system. While optimized for 3D printer enclosures (Voron Trident and Bedslinger) for safe ABS printing, this is a **standalone project** that can be used for any VOC monitoring scenario including workshops, labs, or industrial environments.

### 1.1 Design Goals

- **Standalone Operation**: Independent system, not tightly coupled to Klipper/Raspberry Pi
- **Safety**: Real-time VOC monitoring with visual feedback
- **Reliability**: Dual-zone monitoring with backup sensors
- **Flexibility**: Optional WiFi integration for remote monitoring
- **24/7 Operation**: Independent power system for continuous monitoring
- **Usability**: Local LCD display for immediate feedback

### 1.2 System Architecture

```
┌───────────────────────────────────────┐       ┌─────────────────────────────────────────────────┐
│           CHAMBER (Enclosure)         │       │                  ROOM (Ambient)                 │
│                                       │       │                                                 │
│  ┌─────────────────────────────────┐  │       │  ┌─────────────────┐ ┌───────────┐ ┌─────────┐  │
│  │       ENS160+AHT20              │  │       │  │ ENS160+AHT20    │ │  MQ135    │ │  DHT11  │  │
│  │       (I2C Combo)               │  │       │  │ (I2C Combo)     │ │ (Analog)  │ │ (1-Wire)│  │
│  │                                 │  │       │  │                 │ │           │ │         │  │
│  │  • TVOC (ppb)                   │  │       │  │ • TVOC (ppb)    │ │ • Backup  │ │ • Backup│  │
│  │  • eCO2 (ppm)                   │  │       │  │ • eCO2 (ppm)    │ │   VOC     │ │   Temp  │  │
│  │  • Temperature (°C)             │  │       │  │ • Temp (°C)     │ │ • Toluene │ │ • Backup│  │
│  │  • Humidity (%)                 │  │       │  │ • Humidity (%)  │ │   PPM est │ │   RH    │  │
│  └────────────┬────────────────────┘  │       │  └────────┬────────┘ └─────┬─────┘ └────┬────┘  │
│               │                       │       │           │               │            │       │
└───────────────┼───────────────────────┘       └───────────┼───────────────┼────────────┼───────┘
                │                                           │               │            │
                │                                           │               │            │
┌───────────────▼───────────────────────────────────────────▼───────────────▼────────────▼───────┐
│                                   ESP32 CONTROLLER                                              │
│                                                                                                 │
│   ┌─────────────────────────────────────────────────────────────────────────────────────────┐  │
│   │                              SENSOR INTERFACES                                           │  │
│   │                                                                                          │  │
│   │   ┌───────────────────┐  ┌───────────────────┐  ┌───────────────┐  ┌─────────────────┐  │  │
│   │   │    I2C Bus 1      │  │    I2C Bus 2      │  │    ADC CH1    │  │   GPIO (1-Wire) │  │  │
│   │   │    (Chamber)      │  │     (Room)        │  │    (MQ135)    │  │     (DHT11)     │  │  │
│   │   │                   │  │                   │  │               │  │                 │  │  │
│   │   │   GPIO 21/22      │  │   GPIO 16/17      │  │   GPIO 32     │  │    GPIO 33      │  │  │
│   │   │        ▲          │  │        ▲          │  │       ▲       │  │        ▲        │  │  │
│   │   │        │          │  │        │          │  │       │       │  │        │        │  │  │
│   │   │  ENS160+AHT20     │  │  ENS160+AHT20     │  │  Analog In    │  │   Digital In    │  │  │
│   │   └────────┼──────────┘  └────────┼──────────┘  └───────┼───────┘  └────────┼────────┘  │  │
│   │            │                      │                     │                   │           │  │
│   │            └──────────────────────┴─────────────────────┴───────────────────┘           │  │
│   │                                            │                                             │  │
│   │                                   ┌────────▼────────┐                                    │  │
│   │                                   │  Data Processor │                                    │  │
│   │                                   └────────┬────────┘                                    │  │
│   └────────────────────────────────────────────┼─────────────────────────────────────────────┘  │
│                                                │                                                │
│   ┌────────────────────────────────────────────┼─────────────────────────────────────────────┐  │
│   │                               OUTPUT LAYER                                                │  │
│   │                                            │                                              │  │
│   │          ┌──────────────────┐      ┌───────┴───────┐                                     │  │
│   │          │  ST7920 128x64   │      │  WiFi Client  │                                     │  │
│   │          │  LCD (Serial)    │      │  (Optional)   │                                     │  │
│   │          └──────────────────┘      └───────────────┘                                     │  │
│   └──────────────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Hardware Design

### 2.1 Component Selection

#### 2.1.1 ESP32 Selection Criteria

| Requirement | ESP32 Capability | Notes |
|-------------|------------------|-------|
| Dual I2C Buses | ✓ Hardware I2C on any GPIO | Required for two AHT20 sensors (fixed 0x38 address) |
| ADC Channel | ✓ 12-bit ADC | For MQ135 analog sensor |
| WiFi | ✓ 802.11 b/g/n | Optional remote monitoring |
| Processing Power | ✓ Dual-core 240MHz | LCD GUI + sensor processing |
| RAM | ✓ 520KB SRAM | Sufficient for 128x64 LCD |
| Flash | ✓ 4MB+ | Firmware + configuration storage |

**Recommended Board**: ESP32-WROOM-32 DevKit (38-pin)

### 2.2 Sensor Configuration

| Sensor | Location | Interface | I2C Address | Measurements |
|--------|----------|-----------|-------------|--------------|
| ENS160+AHT20 #1 | Chamber | I2C Bus 1 (GPIO 21/22) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |
| ENS160+AHT20 #2 | Room | I2C Bus 2 (GPIO 16/17) | 0x52, 0x38 | TVOC, eCO2, Temp, RH |
| MQ135 | Room | ADC (GPIO 32) | N/A | Backup VOC (Toluene PPM) |
| DHT11 | Room | 1-Wire (GPIO 33) | N/A | Backup Temp, RH |

**Note**: The MQ135 and DHT11 in the room serve as backup/validation sensors for the ENS160+AHT20. The DHT11 provides temperature and humidity readings used for MQ135 compensation and as a fallback if the primary AHT20 fails.

### 2.3 Memory Considerations

**ESP32 Memory Budget:**
- Total SRAM: 520 KB
- WiFi stack: ~80 KB
- 128x64 LCD framebuffer: ~1 KB
- Application code + libraries: ~50 KB
- **Free heap: ~200+ KB** ✓ Plenty of headroom

Using 128x64 LCD instead of color TFT saves significant memory and simplifies the design.

### 2.4 ESP32 Pin Assignment

```
                         ┌─────────────────────────────┐
                         │         ESP32 DevKit        │
                         │        (38-pin WROOM)       │
                         │                             │
              3.3V ──────┤ 3V3                   GND   ├────── GND
                         │                             │
                         │ EN                   GPIO23 ├────── LCD R/W (Data)
                         │                             │
                         │ GPIO36 (VP)          GPIO22 ├────── I2C1_SCL (Chamber)
                         │                             │
                         │ GPIO39 (VN)          GPIO1  │ (TX0 - Reserved)
                         │                             │
                         │ GPIO34               GPIO3  │ (RX0 - Reserved)
                         │                             │
                         │ GPIO35               GPIO21 ├────── I2C1_SDA (Chamber)
                         │                             │
    MQ135 (Room) ────────┤ GPIO32               GPIO19 │
                         │                             │
    DHT11 (Room) ────────┤ GPIO33               GPIO18 ├────── LCD E (Clock)
                         │                             │
                         │ GPIO25               GPIO5  ├────── LCD RS (CS)
                         │                             │
    USER_BUTTON ─────────┤ GPIO26               GPIO17 ├────── I2C2_SCL (Room)
                         │                             │
                         │ GPIO27               GPIO16 ├────── I2C2_SDA (Room)
                         │                             │
                         │ GPIO14               GPIO4  │ (Free)
                         │                             │
                         │ GPIO12               GPIO2  │
                         │                             │
                         │ GPIO13               GPIO15 │
                         │                             │
                         │ GND                   GND   │
                         │                             │
              5V ────────┤ VIN                   3V3   │
                         │                             │
                         └─────────────────────────────┘
```

#### Pin Assignment Table

| Function | GPIO | Direction | Notes |
|----------|------|-----------|-------|
| **ST7920 LCD (Serial Mode)** |
| LCD_E (Clock) | GPIO 18 | Output | SPI Clock (SCK) |
| LCD_R/W (Data) | GPIO 23 | Output | SPI Data (MOSI) |
| LCD_RS (CS) | GPIO 5 | Output | Chip Select |
| LCD_RST | N/A | N/A | Tied to 5V (not using GPIO) |
| **I2C Bus 1 (Chamber)** |
| I2C1_SDA | GPIO 21 | Bidirectional | ENS160+AHT20 |
| I2C1_SCL | GPIO 22 | Output | 100kHz |
| **I2C Bus 2 (Room)** |
| I2C2_SDA | GPIO 16 | Bidirectional | ENS160+AHT20 |
| I2C2_SCL | GPIO 17 | Output | 100kHz |
| **Analog Input** |
| MQ135 | GPIO 32 | Input (ADC) | Room backup VOC sensor |
| **Digital Input** |
| DHT11 | GPIO 33 | Bidirectional | Room backup Temp/RH sensor |
| **User Input** |
| BUTTON | GPIO 26 | Input | Internal pull-up |

### 2.5 ST7920 LCD Pinout (12864 Display)

The ST7920 is a common 128x64 LCD controller used in 3D printer displays. It supports both parallel (8-bit) and serial (SPI-like) modes. We use **serial mode** to minimize GPIO usage.

```
                    ST7920 128x64 LCD Module (12864)
                   ┌─────────────────────────────────┐
                   │  Pin   Name    Function         │
                   │  ───   ────    ────────         │
                   │   1    GND     Ground           │
                   │   2    VCC     Power (5V)       │
                   │   3    V0      Contrast (NC)    │
                   │   4    RS      Register Select  │◄── GPIO 5 (CS)
                   │   5    R/W     Read/Write       │◄── GPIO 23 (Data)
                   │   6    E       Enable           │◄── GPIO 18 (Clock)
                   │  7-14  DB0-7   Data Bus (NC)    │    (Not used in serial mode)
                   │  15    PSB     Bus Select       │◄── GND (Serial mode)
                   │  16    NC      Not Connected    │
                   │  17    RST     Reset            │◄── 5V (tied high)
                   │  18    VOUT    LCD Drive (NC)   │
                   │  19    BLA     Backlight +      │◄── 5V via 100Ω
                   │  20    BLK     Backlight -      │◄── GND
                   └─────────────────────────────────┘

    Serial Mode Pin Functions:
    ┌─────────┬────────────────────────────────────────────────────────┐
    │ LCD Pin │ Serial Mode Function                                   │
    ├─────────┼────────────────────────────────────────────────────────┤
    │ RS      │ Chip Select (CS) - Pull LOW to start communication     │
    │ R/W     │ Data Line (MOSI) - Serial data input                   │
    │ E       │ Clock (SCK) - Data sampled on rising edge              │
    │ PSB     │ Mode Select - GND = Serial, VCC = Parallel             │
    │ RST     │ Reset - Active LOW, can tie to VCC if not needed       │
    └─────────┴────────────────────────────────────────────────────────┘
```

### 2.6 Circuit Schematics

#### 2.6.1 MQ135 Voltage Divider

```
                    MQ135 Module
                   ┌─────────────┐
                   │             │
            VCC ───┤ VCC     AO  ├───┐
            (5V)   │             │   │
                   │         DO  │   │  (Analog Output 0-5V)
                   │             │   │
            GND ───┤ GND         │   │
                   └─────────────┘   │
                                     │
                              ┌──────┴──────┐
                              │    R1       │
                              │   10kΩ      │
                              └──────┬──────┘
                                     │
                                     ├─────────────► GPIO32 (ESP32 ADC)
                                     │
                              ┌──────┴──────┐
                              │    R2       │
                              │   20kΩ      │
                              └──────┬──────┘
                                     │
                                    GND

    Vout = 5V × 20k / (10k + 20k) = 3.33V max
```

#### 2.6.2 DHT11 Wiring

```
    DHT11 Module                    ESP32
    ┌─────────────┐                ┌─────┐
    │             │                │     │
    │  VCC    ────┼────────────────┤ 3V3 │
    │             │                │     │
    │  DATA   ────┼────────────────┤ 33  │  (GPIO 33)
    │             │                │     │
    │  NC         │                │     │  (Not Connected)
    │             │                │     │
    │  GND    ────┼────────────────┤ GND │
    │             │                │     │
    └─────────────┘                └─────┘

    Note: Most DHT11 modules have a built-in 10kΩ pull-up resistor.
          If using bare DHT11 sensor, add 10kΩ pull-up between DATA and VCC.
```

#### 2.6.3 I2C Bus Connections

```
    I2C BUS 1 (Chamber)                    I2C BUS 2 (Room)
    ═══════════════════                    ════════════════
    
    ESP32          ENS160+AHT20            ESP32          ENS160+AHT20
    ┌─────┐       ┌─────────────┐          ┌─────┐       ┌─────────────┐
    │ 21  ├───────┤ SDA         │          │ 16  ├───────┤ SDA         │
    │ 22  ├───────┤ SCL         │          │ 17  ├───────┤ SCL         │
    │ 3V3 ├───────┤ VCC         │          │ 3V3 ├───────┤ VCC         │
    │ GND ├───────┤ GND         │          │ GND ├───────┤ GND         │
    └─────┘       └─────────────┘          └─────┘       └─────────────┘
    
    WHY TWO I2C BUSES?
    The AHT20 has a FIXED I2C address of 0x38 that cannot be changed.
    To use two AHT20 sensors, we need two separate I2C buses.
```

---

## 3. Display Design

### 3.1 Display Selection: 128x64 OLED/LCD

Using a 128x64 monochrome display (SSD1306 OLED or ST7565 LCD) with the **U8g2 library**.

**U8g2 Font Constraints:**
- Fonts are bitmap-based, not scalable
- Common fonts and their character heights:
  - `u8g2_font_6x10_tf` - 6×10 pixels (fits ~21 chars × 6 lines)
  - `u8g2_font_5x8_tf` - 5×8 pixels (fits ~25 chars × 8 lines)
  - `u8g2_font_4x6_tf` - 4×6 pixels (fits ~32 chars × 10 lines)

### 3.2 Screen 1: Main Data Screen

**Design Principle: Text ON Bars**
- Text is rendered directly on the progress bars
- When bar is filled (solid), text is inverted (white on black)
- When bar is empty, text is normal (black on white)
- This saves vertical space by combining value + bar into one line

**Layout Analysis for 128×64:**

Using `u8g2_font_5x8_tf` (5×8 pixels):
- 128 ÷ 5 = 25 characters per line
- 64 ÷ 8 = 8 lines available

```
┌────────────────────────────┐
│ CHAMBER    │ ROOM          │  Line 1: Header
├────────────┼───────────────┤
│▓▓▓▓▓▓░░░░░│▓░░░░░░░░░░░░░│  Line 2: TVOC bars with text
│ 1245 ppb  │   52 ppb      │         (inverted text on filled part)
├────────────┼───────────────┤
│▓▓▓▓▓▓▓░░░░│▓▓░░░░░░░░░░░░│  Line 3: eCO2 bars with text
│ 2892 ppm  │  421 ppm      │         (inverted text on filled part)
├────────────┼───────────────┤
│ 58°C  23% │ 24°C  45%     │  Line 4: Temp & Humidity
│           │               │
│           │ MQ135: 0.72 ↑ │  Line 5: MQ135 (Room only)
│           │ DHT11: 24°C   │  Line 6: DHT11 backup (Room only)
└────────────┴───────────────┘
        128 × 64 pixels
```

**Bar with Embedded Text Rendering:**
```
Example: TVOC = 1245 ppb (62% of max 2000 ppb)

┌─────────────────────────────────────────────────────────────┐
│▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│  1 2 4 5   p p b              │                             │
│  (inverted/white text)        │  (normal/black text area)   │
└─────────────────────────────────────────────────────────────┘

U8g2 Implementation:
1. Draw filled rectangle for bar portion
2. Set draw mode to XOR or use setDrawColor(0) for inverted text
3. Draw text - it will appear inverted on filled area
```

**Final Layout with Labels (6 lines used):**

Using `u8g2_font_5x8_tf` (5×8 pixels) = 25 chars per line

```
┌─────────────┬──────────────┐
│  CHAMBER    │    ROOM      │  Line 1: Header (8px)
│TVOC▓1245ppb░│TVOC▓52ppb░░░░│  Line 2: TVOC label+bar+value (8px)
│eCO2▓2892ppm░│eCO2▓421ppm░░░│  Line 3: eCO2 label+bar+value (8px)
│ 58°C   23%  │ 24°C   45%   │  Line 4: Temp & RH (8px)
│             │MQ135:0.72 ↑  │  Line 5: MQ135 Room only (8px)
│             │DHT: 24°C 50% │  Line 6: DHT11 Room only (8px)
└─────────────┴──────────────┘
        128 × 64 pixels
```

**Character Count Verification (25 chars max per line):**

Each column is ~12-13 chars (128px ÷ 2 ÷ 5px = 12.8 chars per column)

| Line | Chamber Column | Room Column | Total |
|------|----------------|-------------|-------|
| 1 | "  CHAMBER" (9) | "   ROOM" (7) | 16 ✓ |
| 2 | "TVOC▓1245ppb░" (13) | "TVOC▓52ppb░░░░" (14) | 27 ⚠️ |
| 3 | "eCO2▓2892ppm░" (13) | "eCO2▓421ppm░░░" (14) | 27 ⚠️ |
| 4 | " 58°C   23%" (11) | " 24°C   45%" (11) | 22 ✓ |
| 5 | "" (0) | "MQ135:0.72 ↑" (12) | 12 ✓ |
| 6 | "" (0) | "DHT: 24°C 50%" (13) | 13 ✓ |

**Issue:** Lines 2-3 are slightly over 25 chars. Solutions:
1. Use smaller font `u8g2_font_4x6_tf` (32 chars per line) ✓
2. Abbreviate: "TVOC" → "TV", "eCO2" → "CO2"
3. Remove spaces

**Revised with 4x6 font (32 chars per line):**

Using Option A layout with outer borders and abbreviated labels:

```
┌─────────────────────────────────┐  ← Top border (1px)
│   CHAMBER     │     ROOM        │  Line 1: Header
│TVOC ▓▓1245ppb░│TVOC ▓52ppb░░░░░░│  Line 2: TVOC bar
│eCO2 ▓▓2892ppm░│eCO2 ▓421ppm░░░░░│  Line 3: eCO2 bar
│T:58°C  RH:23% │T:24°C  RH:45%   │  Line 4: AHT20 T & RH
├─────────────────────────────────┤  Line 5: Full-width separator
│ Room Backup                     │  Line 6: Section header (full-width)
│ Toluene:2.5ppm T:24°C RH:50%    │  Line 7: MQ135→Toluene + DHT11 (full-width)
└─────────────────────────────────┘  ← Bottom border (1px)
        128 × 64 pixels (using 4x6 font)
        7 lines × 8px = 56px content + 8px for borders = 64px ✓
```

**Label Abbreviations:**
- `T:` instead of `Temp:` (consistent across all screens)
- `RH:` for relative humidity
- `Toluene:` for MQ135 estimated PPM (full name, sufficient space)

**Character Count Verification (32 chars max per line with 4x6 font):**
- Line 7: `Toluene:999ppm T:99°C RH:99%` = 29 chars ✓ (fits with room to spare)

**Verdict: FITS with 4x6 font and outer borders** ✓

### 3.2.1 MQ135 Toluene PPM Estimation

The MQ135 analog sensor outputs a resistance ratio (Rs/Ro) which can be converted to an estimated PPM value using the **Toluene calibration curve** from the datasheet. Toluene is used as a proxy for VOCs emitted during 3D printing (styrene from ABS, etc.).

**Conversion Formula:**
```
PPM = a × (Rs/Ro)^b

Where for Toluene:
  a = 44.947
  b = -3.445
```

**Temperature/Humidity Compensation:**

The MQ135 sensitivity varies with temperature and humidity. Apply correction factor:

```
Correction Factor = CORA × T² + CORB × T + CORC - (RH - 33) × CORD

Where:
  CORA = 0.00035
  CORB = 0.02718
  CORC = 1.39538
  CORD = 0.0018
  T = Temperature in °C (from DHT11)
  RH = Relative Humidity in % (from DHT11)

Corrected Rs/Ro = (Rs/Ro) / Correction Factor
```

**Implementation:**
```cpp
float calculateToluenePPM(float rsRo, float tempC, float humidity) {
    // Temperature/humidity correction
    float correction = 0.00035 * tempC * tempC 
                     + 0.02718 * tempC 
                     + 1.39538 
                     - (humidity - 33.0) * 0.0018;
    
    float correctedRsRo = rsRo / correction;
    
    // Toluene curve: PPM = 44.947 × (Rs/Ro)^(-3.445)
    float ppm = 44.947 * pow(correctedRsRo, -3.445);
    
    // Clamp to reasonable range
    if (ppm < 0.1) ppm = 0.1;
    if (ppm > 999.0) ppm = 999.0;
    
    return ppm;
}
```

**Gas Calibration Curves (from MQ135 datasheet):**

| Gas | a | b | Use Case |
|-----|---|---|----------|
| **Toluene** | 44.947 | -3.445 | ✓ VOC/Styrene approximation (used) |
| Acetone | 34.668 | -3.369 | Alternative VOC proxy |
| CO2 | 116.602 | -2.769 | Not a VOC - don't use for VOC |
| NH3 | 102.2 | -2.473 | Ammonia detection |
| CO | 605.18 | -3.937 | Carbon monoxide |

**Important Notes:**
- This is an **estimation**, not a precise measurement
- MQ135 cannot distinguish between different VOCs
- Toluene curve provides a reasonable approximation for aromatic VOCs
- Values should be interpreted as relative indicators, not absolute concentrations

**What We Display:**
```
Tol:2.5ppm
    │
    └── Estimated Toluene-equivalent PPM (0.1-999 range)
```

### 3.3 Screen 2: Status Screen

All WiFi, connectivity, and system status on this page:

```
┌────────────────────────────┐
│ SYSTEM STATUS              │  Line 1: Header
├────────────────────────────┤
│ WiFi: Connected            │  Line 2: WiFi status
│ IP: 192.168.1.105          │  Line 3: Device IP address
│                            │  Line 4: (blank)
│ Klipper Push: Enabled      │  Line 5: Push flag
│ Host: 192.168.1.100        │  Line 6: Klipper host/IP
│ Last: OK                   │  Line 7: Last push result
│ Free Mem: 142384           │  Line 8: Free heap memory
└────────────────────────────┘
```

**Verdict: FITS** ✓

### 3.4 Screen State Machine

```
┌─────────────────┐
│   SCREEN OFF    │◄─────────────────────────────────┐
│   (Backlight    │                                   │
│    off, LCD     │                                   │
│    still runs)  │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         ▼                                            │
┌─────────────────┐                                   │
│   MAIN SCREEN   │                                   │
│                 │                                   │
│ • Chamber TVOC  │                                   │
│ • Chamber eCO2  │                                   │
│ • Room TVOC     │                                   │
│ • Room eCO2     │                                   │
│ • Temp/RH       │                                   │
│ • MQ135 trend   │                                   │
│ • WiFi/Push     │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         ▼                                            │
┌─────────────────┐                                   │
│  STATUS SCREEN  │                                   │
│                 │                                   │
│ • WiFi status   │                                   │
│ • IP address    │                                   │
│ • Push enabled  │                                   │
│ • Last push     │                                   │
│ • Free memory   │                                   │
└────────┬────────┘                                   │
         │ Button Press                               │
         └────────────────────────────────────────────┘
```

### 3.5 No Time Display

**Why no clock?**
- ESP32 has no RTC (Real-Time Clock) hardware
- Cannot assume internet access for NTP time sync
- Local network may not allow public internet access
- Adding RTC module adds complexity and cost

**Alternative**: Show "uptime" (time since boot) on status screen if needed.

---

## 4. Software Design

### 4.1 Software Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ESP32 SOFTWARE ARCHITECTURE                       │
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
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │  │
│  │  │ ENS160   │  │  AHT20   │  │  MQ135   │  │    U8g2 LCD      │   │  │
│  │  │ Driver   │  │  Driver  │  │  Driver  │  │    Display       │   │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘   │  │
│  │                                                                    │  │
│  │  ┌──────────────────────────────────────────────────────────────┐ │  │
│  │  │                     Button Handler                            │ │  │
│  │  └──────────────────────────────────────────────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                    HAL / PLATFORM LAYER                            │  │
│  │                                                                    │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐          │  │
│  │  │   I2C    │  │   SPI    │  │   ADC    │  │  GPIO    │          │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘          │  │
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
│   ├── main.cpp                # Entry point
│   ├── sensors/
│   │   ├── sensor_manager.cpp
│   │   ├── ens160_driver.cpp
│   │   ├── aht20_driver.cpp
│   │   └── mq135_driver.cpp
│   ├── display/
│   │   ├── display_manager.cpp
│   │   └── screens.cpp
│   ├── network/
│   │   └── wifi_manager.cpp
│   └── input/
│       └── button_handler.cpp
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
│  - ADC          │
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

[env:esp32dev]
platform = espressif32
board = esp32dev
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

# Upload to ESP32
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
| WiFi | ESP32 WiFi (built-in) |

---

## 6. Bill of Materials

### 6.1 Required Components

| Component | Model/Spec | Qty | Notes |
|-----------|------------|-----|-------|
| Microcontroller | ESP32-WROOM-32 DevKit | 1 | 38-pin |
| VOC Sensor Module | ENS160 + AHT20 Combo | 2 | I2C |
| Gas Sensor | MQ135 Module | 1 | Room only |
| Display | 128x64 OLED (SSD1306) | 1 | SPI or I2C |
| Button | 6mm Tactile Switch | 1 | Momentary |
| Resistor | 10kΩ 1/4W | 1 | Voltage divider |
| Resistor | 20kΩ 1/4W | 1 | Voltage divider |
| Power Supply | 5V 1A USB | 1 | |

### 6.2 Wire Count Summary

| Connection | Wires |
|------------|-------|
| LCD (SPI) | 5 (CS, DC, RST, MOSI, SCK) + 2 (VCC, GND) = 7 |
| I2C Bus 1 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| I2C Bus 2 | 2 (SDA, SCL) + 2 (VCC, GND) = 4 |
| MQ135 | 1 (AO) + 2 (VCC, GND) = 3 |
| Button | 1 (GPIO) + 1 (GND) = 2 |
| **Total** | **~20 wires** |

---

## 7. Assembly & Testing

### 7.1 Assembly Checklist

- [ ] Wire I2C Bus 1 (GPIO 21/22) to Chamber ENS160+AHT20
- [ ] Wire I2C Bus 2 (GPIO 16/17) to Room ENS160+AHT20
- [ ] Build MQ135 voltage divider (10k + 20k)
- [ ] Connect MQ135 to GPIO 32
- [ ] Connect LCD via SPI
- [ ] Wire button between GPIO 26 and GND
- [ ] Connect 5V power

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
- [MQ135 Datasheet](https://www.winsen-sensor.com/sensors/voc-sensor/mq135.html)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [ESP32 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)