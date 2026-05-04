#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// VoC Monitor Configuration - ESP8266 (NodeMCU)
// =============================================================================

// -----------------------------------------------------------------------------
// Pin Definitions - NodeMCU v3 (ESP-12E)
// -----------------------------------------------------------------------------

// ST7920 LCD (128x64) - Software SPI Mode
#define LCD_CLK   16    // Clock (E pin) - D0
#define LCD_DATA  13    // Data (R/W pin) - D7
#define LCD_CS    15    // Chip Select (RS pin) - D8
#define LCD_RST   2     // Reset pin - D4

// I2C Bus 1 - Hardware I2C - Chamber sensors (ENS160 + AHT20)
#define I2C1_SDA  4     // D2
#define I2C1_SCL  5     // D1

// I2C Bus 2 - Software I2C - Room sensors (ENS160 + AHT20)
#define I2C2_SDA  12    // D6
#define I2C2_SCL  14    // D5

// User Input
#define BUTTON_PIN 0    // D3 - FLASH button (active LOW with internal pull-up)

// LCD Backlight Control
#define LCD_BACKLIGHT 3 // D9/RX - Backlight control (HIGH = on, LOW = off)

// -----------------------------------------------------------------------------
// I2C Addresses
// -----------------------------------------------------------------------------
#define ENS160_ADDR  0x52   // ENS160 VOC sensor
#define AHT20_ADDR   0x38   // AHT20 temperature/humidity sensor

// -----------------------------------------------------------------------------
// Display Settings
// -----------------------------------------------------------------------------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Screen states
enum ScreenState {
    SCREEN_MAIN = 0,       // Main sensor display (backlight ON)
    SCREEN_STATUS = 1,     // Status/WiFi info (backlight ON)
    SCREEN_MAIN_DARK = 2   // Main sensor display (backlight OFF)
};

// -----------------------------------------------------------------------------
// Sensor Thresholds
// -----------------------------------------------------------------------------
#define TVOC_MAX      2000    // ppb - max for bar graph
#define ECO2_MAX      5000    // ppm - max for bar graph
#define TVOC_WARNING  500     // ppb - warning threshold
#define ECO2_WARNING  1000    // ppm - warning threshold

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
#define SENSOR_READ_INTERVAL  1000   // ms - read sensors every 1 second
#define DISPLAY_UPDATE_INTERVAL 500  // ms - update display every 500ms
#define BUTTON_DEBOUNCE_MS    250    // ms - button debounce time (increased for reliable detection)

// -----------------------------------------------------------------------------
// WiFi Configuration
// -----------------------------------------------------------------------------
// Set to 1 to enable WiFi, 0 to disable
#define WIFI_ENABLED          1

// Set to 1 to push data to Klipper/Moonraker, 0 to disable
// Requires WIFI_ENABLED = 1
#define KLIPPER_PUSH_ENABLED  0

// Klipper/Moonraker host (IP or hostname)
#define KLIPPER_HOST          "kbox.local"
#define KLIPPER_PORT          7125

// Push interval (ms) - how often to send data to Klipper
#define KLIPPER_PUSH_INTERVAL 5000

// WiFi credentials are in credentials.h (gitignored):
//   #define SSID "your_ssid"
//   #define PASSWD "your_password"

#endif // CONFIG_H