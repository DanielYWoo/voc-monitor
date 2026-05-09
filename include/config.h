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
#define LCD_CS    0     // Chip Select (RS pin) - D3 (GPIO 0) - swapped with buzzer
#define LCD_RST   2     // Reset pin - D4

// I2C Bus 1 - Hardware I2C - Chamber sensors (ENS160 + AHT20)
#define I2C1_SDA  4     // D2
#define I2C1_SCL  5     // D1

// I2C Bus 2 - Software I2C - Room sensors (ENS160 + AHT20)
#define I2C2_SDA  12    // D6
#define I2C2_SCL  14    // D5

// User Input - Button on TX (GPIO 1) with interrupt
#define BUTTON_PIN    1     // TX (GPIO 1) - Button with internal pull-up, interrupt on FALLING edge
                            // Note: Serial TX is disabled when using this pin for button

// LCD Backlight Control
#define LCD_BACKLIGHT 3     // RX (GPIO 3) - Backlight control (HIGH = on, LOW = off)

// Buzzer (Passive buzzer for Geiger counter effect)
#define BUZZER_PIN    15    // D8 (GPIO 15) - Passive buzzer (swapped with LCD_CS to avoid GPIO 0 boot issue)

// -----------------------------------------------------------------------------
// I2C Addresses
// -----------------------------------------------------------------------------
#define ENS160_ADDR      0x52   // ENS160 VOC sensor (default address, ADDR pin LOW)
#define ENS160_ADDR_ALT  0x53   // ENS160 VOC sensor (alternate address, ADDR pin HIGH)
#define AHT20_ADDR       0x38   // AHT20 temperature/humidity sensor

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
#define TVOC_MAX      2000    // ppb - max for Geiger scaling (fastest clicking)
#define ECO2_MAX      5000    // ppm - max for Geiger scaling (fastest clicking)
#define TVOC_WARNING  60      // ppb - warning threshold (Geiger starts clicking)
#define ECO2_WARNING  500     // ppm - warning threshold (Geiger starts clicking)

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