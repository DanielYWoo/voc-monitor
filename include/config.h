#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// VoC Monitor Configuration
// =============================================================================

// -----------------------------------------------------------------------------
// Pin Definitions - ESP32-WROOM-32 DevKit (38-pin)
// -----------------------------------------------------------------------------

// SPI LCD (128x64 ST7920)
#define LCD_SCK   18    // SPI Clock (E pin)
#define LCD_MOSI  23    // SPI Data (R/W pin)
#define LCD_CS    5     // Chip Select (RS pin)
// LCD_RST is tied to 5V directly, not using GPIO

// I2C Bus 1 - Chamber sensors (ENS160 + AHT20)
#define I2C1_SDA  21
#define I2C1_SCL  22

// I2C Bus 2 - Room sensors (ENS160 + AHT20)
#define I2C2_SDA  16
#define I2C2_SCL  17

// Analog Input
#define MQ135_PIN 32    // MQ135 analog output (via voltage divider)

// DHT11 Backup sensor (Room)
#define DHT11_PIN 33    // DHT11 data pin

// User Input
#define BUTTON_PIN 26   // Tactile button (active LOW with internal pull-up)

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
    SCREEN_OFF = 0,
    SCREEN_MAIN = 1,
    SCREEN_STATUS = 2
};

// -----------------------------------------------------------------------------
// Sensor Thresholds
// -----------------------------------------------------------------------------
#define TVOC_MAX      2000    // ppb - max for bar graph
#define ECO2_MAX      5000    // ppm - max for bar graph
#define TVOC_WARNING  500     // ppb - warning threshold
#define ECO2_WARNING  1000    // ppm - warning threshold

// -----------------------------------------------------------------------------
// MQ135 Toluene Calibration Constants
// -----------------------------------------------------------------------------
#define MQ135_TOLUENE_A   44.947f   // Toluene curve coefficient a
#define MQ135_TOLUENE_B   -3.445f   // Toluene curve exponent b

// Temperature/Humidity correction constants
#define MQ135_CORA  0.00035f
#define MQ135_CORB  0.02718f
#define MQ135_CORC  1.39538f
#define MQ135_CORD  0.0018f

// MQ135 calibration
#define MQ135_RO_CLEAN_AIR  9.83f   // Ro in clean air (calibrate for your sensor)
#define MQ135_RL            10.0f   // Load resistance in kOhm

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
#define SENSOR_READ_INTERVAL  1000   // ms - read sensors every 1 second
#define DISPLAY_UPDATE_INTERVAL 500  // ms - update display every 500ms
#define BUTTON_DEBOUNCE_MS    50     // ms - button debounce time

// -----------------------------------------------------------------------------
// WiFi (Optional)
// -----------------------------------------------------------------------------
// WiFi credentials should be in credentials.h (gitignored)
// #define WIFI_SSID "your_ssid"
// #define WIFI_PASSWORD "your_password"

#endif // CONFIG_H