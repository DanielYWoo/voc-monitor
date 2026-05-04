# ESP8266 + ILI9341 2.2" TFT VoC Monitor

A VOC (Volatile Organic Compounds) monitoring system using ESP8266 NodeMCU with a 2.2" ILI9341 TFT display.

## Hardware Components

- **MCU**: NodeMCU v3 ESP8266 E12
- **Display**: 2.2" TFT SPI ILI9341 (MSP2202) - 320x240 pixels, 65K colors
- **Sensors**: ENS160 + AHT20 (I2C), MQ135 (Analog), DHT11 (Backup)

## ESP8266 NodeMCU v3 Pinout

```
                    ESP8266 E12 NodeMCU v3 Pinout
                    (CS tied to GND configuration)
    
                   ┌─────────────────────┐
        A0  (ADC0) │●  ◄── MQ135        ●│ D0 (GPIO16) ◄── DHT11
        (reserved) │●                   ●│ D1 (GPIO5)  ◄── I2C SCL
        (reserved) │●                   ●│ D2 (GPIO4)  ◄── I2C SDA
        SD3(GPIO10)│●                   ●│ D3 (GPIO0)  ◄── TFT DC
         SD2(GPIO9)│●                   ●│ D4 (GPIO2)  ◄── TFT RST
   SD1/MOSI(GPIO8) │●                   ●│ 3V3         ◄── TFT VCC, LED
    CMD/CS(GPIO11) │●                   ●│ GND         ◄── TFT GND, CS
   SD0/MISO(GPIO7) │●                   ●│ D5 (GPIO14) ◄── TFT SCK
        CLK(GPIO6) │●                   ●│ D6 (GPIO12)     (free)
               GND │●                   ●│ D7 (GPIO13) ◄── TFT MOSI
               3V3 │●                   ●│ D8 (GPIO15)     (free)
                EN │●                   ●│ RX (GPIO3)
               RST │●                   ●│ TX (GPIO1)
               GND │●                   ●│ GND
               VIN │●                   ●│ 3V3
                   └─────────────────────┘
                   │       │ USB │       │
                   └───────┴─────┴───────┘

    TFT Connections (CS tied to GND):
    - VCC  → 3V3
    - GND  → GND  
    - CS   → GND (tied, always selected)
    - RST  → D4 (GPIO2)
    - DC   → D3 (GPIO0)
    - MOSI → D7 (GPIO13)
    - SCK  → D5 (GPIO14)
    - LED  → 3V3
    - MISO → NC (not connected)
```

## ILI9341 2.2" TFT Display Pinout (MSP2202)

Reference: https://www.lcdwiki.com/2.2inch_SPI_Module_ILI9341_SKU:MSP2202

```
                    ILI9341 2.2" TFT Module (MSP2202)
                   ┌─────────────────────────────────────┐
                   │  Pin   Label      Description       │
                   │  ───   ─────      ───────────       │
                   │   1    VCC        3.3V/5V Power     │◄── 3V3
                   │   2    GND        Ground            │◄── GND
                   │   3    CS         Chip Select (LOW) │◄── GND (tied, always selected)
                   │   4    RESET      Reset (LOW)       │◄── D4 (GPIO2)
                   │   5    DC/RS      Data/Command      │◄── D3 (GPIO0)
                   │   6    SDI/MOSI   SPI Data In       │◄── D7 (GPIO13)
                   │   7    SCK        SPI Clock         │◄── D5 (GPIO14)
                   │   8    LED        Backlight (HIGH)  │◄── 3V3 (always on)
                   │   9    SDO/MISO   SPI Data Out      │◄── NC (not connected)
                   └─────────────────────────────────────┘
```

## Wiring Diagram

### Option A: CS tied to GND (Recommended - saves 1 GPIO)

If the ILI9341 is the **only SPI device** (not using SD card slot), tie CS to GND:

```
    ILI9341 TFT Module             ESP8266 (NodeMCU v3)
    ┌─────────────┐                ┌─────┐
    │             │                │     │
    │  VCC (1) ───┼────────────────┤ 3V3 │  (3.3V Power)
    │  GND (2) ───┼───┬────────────┤ GND │
    │  CS  (3) ───┼───┘            │     │  (CS tied to GND - always selected)
    │  RST (4) ───┼────────────────┤ D4  │  (GPIO2)
    │  DC  (5) ───┼────────────────┤ D3  │  (GPIO0)
    │  MOSI(6) ───┼────────────────┤ D7  │  (GPIO13, Hardware SPI)
    │  SCK (7) ───┼────────────────┤ D5  │  (GPIO14, Hardware SPI)
    │  LED (8) ───┼────────────────┤ 3V3 │  (Backlight always on)
    │  MISO(9) ───┼────────────────┤ NC  │  (Not connected - not reading from LCD)
    │             │                │     │
    └─────────────┘                └─────┘
```

### Option B: CS controlled by GPIO (if using multiple SPI devices)

```
    ILI9341 TFT Module             ESP8266 (NodeMCU v3)
    ┌─────────────┐                ┌─────┐
    │             │                │     │
    │  VCC (1) ───┼────────────────┤ 3V3 │  (3.3V Power)
    │  GND (2) ───┼────────────────┤ GND │
    │  CS  (3) ───┼────────────────┤ D8  │  (GPIO15)
    │  RST (4) ───┼────────────────┤ D4  │  (GPIO2)
    │  DC  (5) ───┼────────────────┤ D3  │  (GPIO0)
    │  MOSI(6) ───┼────────────────┤ D7  │  (GPIO13, Hardware SPI)
    │  SCK (7) ───┼────────────────┤ D5  │  (GPIO14, Hardware SPI)
    │  LED (8) ───┼────────────────┤ 3V3 │  (Backlight always on)
    │  MISO(9) ───┼────────────────┤ D6  │  (GPIO12, optional)
    │             │                │     │
    └─────────────┘                └─────┘
```

## Complete System Wiring (CS tied to GND)

```
    ┌─────────────────────────────────────────────────────────────────┐
    │                    ESP8266 NodeMCU v3                           │
    │                                                                 │
    │  3V3 ──┬──────────────────────────────────────────────────────┐ │
    │        │                                                      │ │
    │        ├── TFT VCC (Pin 1)                                    │ │
    │        ├── TFT LED (Pin 8) [Backlight]                        │ │
    │        └── ENS160/AHT20 VCC                                   │ │
    │                                                               │ │
    │  GND ──┬──────────────────────────────────────────────────────┤ │
    │        ├── TFT GND (Pin 2)                                    │ │
    │        ├── TFT CS (Pin 3)  [tied to GND - always selected]    │ │
    │        └── ENS160/AHT20 GND                                   │ │
    │                                                               │ │
    │  D4 (GPIO2)  ─── TFT RST (Pin 4)    [Reset]                   │ │
    │  D3 (GPIO0)  ─── TFT DC (Pin 5)     [Data/Command]            │ │
    │  D7 (GPIO13) ─── TFT MOSI (Pin 6)   [SPI Data]                │ │
    │  D5 (GPIO14) ─── TFT SCK (Pin 7)    [SPI Clock]               │ │
    │                                                               │ │
    │  D1 (GPIO5)  ─── I2C SCL ─── ENS160/AHT20 SCL                 │ │
    │  D2 (GPIO4)  ─── I2C SDA ─── ENS160/AHT20 SDA                 │ │
    │                                                               │ │
    │  A0 ─────────── MQ135 Analog Out (via voltage divider)        │ │
    │  D0 (GPIO16) ─── DHT11 Data (with 4.7k pull-up)               │ │
    │                                                               │ │
    │  D6 (GPIO12) ─── (free)                                       │ │
    │  D8 (GPIO15) ─── (free)                                       │ │
    │                                                               │ │
    └─────────────────────────────────────────────────────────────────┘
```

## Pin Assignment Summary (CS tied to GND)

| Function | NodeMCU Pin | GPIO | Notes |
|----------|-------------|------|-------|
| **TFT Display (SPI)** | | | |
| TFT CS | GND | - | Tied to GND (always selected) |
| TFT RESET | D4 | GPIO2 | Reset (active LOW), must be HIGH at boot |
| TFT DC/RS | D3 | GPIO0 | Data/Command, must be HIGH at boot |
| TFT MOSI | D7 | GPIO13 | Hardware SPI MOSI |
| TFT SCK | D5 | GPIO14 | Hardware SPI Clock |
| TFT MISO | NC | - | Not connected (not reading from display) |
| TFT LED | 3V3 | - | Backlight (always on) |
| **I2C Sensors** | | | |
| I2C SCL | D1 | GPIO5 | ENS160 + AHT20 |
| I2C SDA | D2 | GPIO4 | ENS160 + AHT20 |
| **Analog Sensor** | | | |
| MQ135 | A0 | ADC0 | Requires voltage divider (5V→1V) |
| **Backup Sensor** | | | |
| DHT11 | D0 | GPIO16 | Temp/Humidity backup (needs external 4.7k pull-up) |
| **Free Pins** | | | |
| (available) | D6 | GPIO12 | Free for future use |
| (available) | D8 | GPIO15 | Free for future use |

### Pin Conflict Notes

With CS tied to GND, the TFT only uses D3, D4, D5, D7 (4 GPIO pins instead of 6):
- **DHT11** moved from D4 to D0 (GPIO16) - requires external 4.7k pull-up resistor
- **Button** moved from D3 to TX (GPIO1) - only usable if serial monitor is disabled
- **Alternative**: Use the built-in FLASH button on NodeMCU (connected to GPIO0/D3) when TFT is idle

## Critical Notes

### 1. ESP8266 Boot-Sensitive Pins

The ESP8266 has several pins that affect boot behavior:

| Pin | Boot Requirement | Our Usage | Status |
|-----|------------------|-----------|--------|
| GPIO0 (D3) | HIGH = Normal boot | TFT DC | ✅ OK - Display doesn't pull low at boot |
| GPIO2 (D4) | HIGH = Normal boot | TFT RESET | ✅ OK - We want RESET high anyway |
| GPIO15 (D8) | LOW = Normal boot | TFT CS | ✅ OK - CS is active LOW, idle HIGH |

### 2. Hardware SPI Pins (Fixed on ESP8266)

The ESP8266 has dedicated hardware SPI pins that provide better performance:
- **MOSI**: GPIO13 (D7) - Cannot be changed
- **MISO**: GPIO12 (D6) - Cannot be changed  
- **SCK**: GPIO14 (D5) - Cannot be changed
- **CS**: Any GPIO (we use GPIO15/D8)

### 3. Voltage Levels

- **ESP8266**: 3.3V logic
- **ILI9341 Module**: 3.3V logic (with onboard regulator for 5V VCC option)
- **No level shifters needed** when powering display from 3.3V

### 4. Power Requirements

- **ESP8266**: ~80mA average, 170mA peak (WiFi TX)
- **ILI9341 TFT**: ~50mA with backlight
- **Total**: ~250mA peak - USB power is sufficient

### 5. Alternative Pin Configuration

If you experience boot issues with GPIO15 (D8) as CS:

```
Alternative 1: Move CS to D1 (GPIO5)
- TFT CS → D1 (GPIO5)
- I2C SCL → D0 (GPIO16) [Note: No hardware I2C on GPIO16]

Alternative 2: Move CS to D2 (GPIO4)  
- TFT CS → D2 (GPIO4)
- I2C SDA → D0 (GPIO16) [Note: Requires software I2C]
```

## Software Configuration

### platformio.ini

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200

lib_deps = 
    bodmer/TFT_eSPI@^2.5.0
    adafruit/Adafruit AHTX0@^2.0.0
    sparkfun/SparkFun Indoor Air Quality Sensor - ENS160@^1.0.0
```

### TFT_eSPI User Setup (User_Setup.h)

Create or modify `User_Setup.h` in the TFT_eSPI library folder:

#### Option A: CS tied to GND (Recommended)

```cpp
// User_Setup.h for ESP8266 + ILI9341 2.2" TFT
// CS tied to GND - saves GPIO15

#define USER_SETUP_INFO "ESP8266_ILI9341_2.2inch_NoCS"

// Driver
#define ILI9341_DRIVER

// Resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ESP8266 Pin Definitions - CS tied to GND
#define TFT_CS   -1  // CS tied to GND (always selected)
#define TFT_DC    0  // GPIO0  (D3)
#define TFT_RST   2  // GPIO2  (D4)
#define TFT_MOSI 13  // GPIO13 (D7) - Hardware SPI
#define TFT_SCLK 14  // GPIO14 (D5) - Hardware SPI
// TFT_MISO not needed - not reading from display

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// Performance
#define SPI_FREQUENCY  40000000  // 40MHz (max for ESP8266)
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

#### Option B: CS controlled by GPIO15

```cpp
// User_Setup.h for ESP8266 + ILI9341 2.2" TFT
// CS controlled by GPIO - use if multiple SPI devices

#define USER_SETUP_INFO "ESP8266_ILI9341_2.2inch"

// Driver
#define ILI9341_DRIVER

// Resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ESP8266 Pin Definitions
#define TFT_CS   15  // GPIO15 (D8)
#define TFT_DC    0  // GPIO0  (D3)
#define TFT_RST   2  // GPIO2  (D4)
#define TFT_MOSI 13  // GPIO13 (D7) - Hardware SPI
#define TFT_SCLK 14  // GPIO14 (D5) - Hardware SPI
#define TFT_MISO 12  // GPIO12 (D6) - Hardware SPI (optional)

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// Performance
#define SPI_FREQUENCY  40000000  // 40MHz (max for ESP8266)
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

### Basic Test Code

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    delay(100);
    
    tft.init();
    tft.setRotation(1);  // Landscape mode (320x240)
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Hello World!");
    
    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.println("ESP8266 + ILI9341 2.2\" TFT");
    tft.println("Resolution: 320x240");
}

void loop() {
    // Draw a simple animation
    static int x = 0;
    tft.fillRect(x, 100, 10, 10, TFT_BLACK);
    x = (x + 5) % 310;
    tft.fillRect(x, 100, 10, 10, TFT_GREEN);
    delay(50);
}
```

## Build & Upload

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor serial output
pio device monitor
```

## Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| White/blank screen | Wrong pins or SPI not working | Verify all pin connections, check User_Setup.h |
| Display shows garbage | Wrong driver or rotation | Confirm ILI9341_DRIVER is defined |
| ESP8266 won't boot | GPIO0/2/15 pulled wrong | Check boot pin states, disconnect TFT during upload |
| Flickering display | Power issue | Add 100µF capacitor on VCC, use shorter wires |
| Colors inverted | Display variant difference | Try `tft.invertDisplay(true);` |
| Upload fails | GPIO0 held low | Disconnect D3 (DC) during upload, or hold FLASH button |

## References

- [ILI9341 2.2" TFT Module (MSP2202)](https://www.lcdwiki.com/2.2inch_SPI_Module_ILI9341_SKU:MSP2202)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ESP8266 NodeMCU Pinout](https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/)