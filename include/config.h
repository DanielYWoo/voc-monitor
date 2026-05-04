#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// VoC Monitor Configuration - ESP8266 ESP-12E Version
// =============================================================================

// -----------------------------------------------------------------------------
// Pin Definitions - NodeMCU ESP8266 / Wemos D1 Mini
// -----------------------------------------------------------------------------

// ILI9341 TFT (320x240) - Hardware SPI Mode
// Reference: https://www.lcdwiki.com/2.2inch_SPI_Module_ILI9341_SKU:MSP2202
// NodeMCU Hardware SPI: D5=GPIO14(SCK), D6=GPIO12(MISO), D7=GPIO13(MOSI)
//
// Option A: CS tied to GND (recommended - saves GPIO15)
#define TFT_CS    -1    // CS tied to GND (always selected, saves 1 GPIO)
// Option B: CS controlled by GPIO (uncomment if using multiple SPI devices)
// #define TFT_CS    15    // Chip Select - D8 (GPIO15)
//
#define TFT_RST    2    // Reset - D4 (GPIO2), must be HIGH at boot
#define TFT_DC     0    // Data/Command - D3 (GPIO0), must be HIGH at boot
#define TFT_MOSI  13    // Hardware SPI MOSI - D7 (GPIO13)
#define TFT_SCLK  14    // Hardware SPI Clock - D5 (GPIO14)
// #define TFT_MISO  12    // Hardware SPI MISO - D6 (GPIO12), not needed if not reading
// TFT LED (backlight) is tied to 3.3V (always on)

// I2C Bus - ENS160 + AHT20
// NodeMCU pins: D1=GPIO5 (SCL), D2=GPIO4 (SDA)
#define I2C_SDA   4     // D2
#define I2C_SCL   5     // D1

// Analog Input - MQ135
// ESP8266 has only ONE ADC pin (A0), range 0-1V
#define MQ135_PIN A0    // Analog input (use voltage divider!)

// DHT11 Backup sensor
// NodeMCU pin: D0=GPIO16 (moved from D4 which is now used by TFT_RST)
#define DHT11_PIN 16    // D0 - Note: No internal pull-up, add external 4.7k pull-up

// User Input - Button
// Note: D3 (GPIO0) is now used by TFT_DC
// Using the FLASH button on NodeMCU (directly connected to GPIO0)
// Or connect external button to TX (GPIO1) if serial not needed
#define BUTTON_PIN 1    // TX (GPIO1) - only if serial monitor not needed
// Alternative: Use GPIO16 (D0) if DHT11 not used, or omit button

// -----------------------------------------------------------------------------
// I2C Addresses
// -----------------------------------------------------------------------------
#define ENS160_ADDR  0x52   // ENS160 VOC sensor
#define AHT20_ADDR   0x38   // AHT20 temperature/humidity sensor

// -----------------------------------------------------------------------------
// Display Settings
// -----------------------------------------------------------------------------
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

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

// ESP8266 ADC specifics
// ADC range is 0-1V (10-bit = 0-1023)
// Voltage divider: 100k + 22k gives 0.9V max from 5V input
#define MQ135_ADC_MAX       1023    // 10-bit ADC
#define MQ135_ADC_VREF      1.0f    // ESP8266 ADC reference voltage
#define MQ135_DIVIDER_RATIO 5.545f  // (100k + 22k) / 22k = 5.545

// -----------------------------------------------------------------------------
// Calibration Mode
// -----------------------------------------------------------------------------
// Uncomment to run MQ135 calibration on boot (one-time use in clean air)
// After calibration, comment out and re-upload for normal operation
// #define CALIBRATION_MODE

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
#define SENSOR_READ_INTERVAL  1000   // ms - read sensors every 1 second
#define DISPLAY_UPDATE_INTERVAL 500  // ms - update display every 500ms
#define BUTTON_DEBOUNCE_MS    50     // ms - button debounce time

// -----------------------------------------------------------------------------
// WiFi Configuration
// -----------------------------------------------------------------------------
// Set to 1 to enable WiFi, 0 to disable
#define WIFI_ENABLED          0

// Set to 1 to push data to Klipper/Moonraker, 0 to disable
// Requires WIFI_ENABLED = 1
#define KLIPPER_PUSH_ENABLED  0

// Klipper/Moonraker host (IP or hostname)
#define KLIPPER_HOST          "192.168.1.100"
#define KLIPPER_PORT          7125

// Push interval (ms) - how often to send data to Klipper
#define KLIPPER_PUSH_INTERVAL 5000

// WiFi credentials are in credentials.h (gitignored):
//   #define SSID "your_ssid"
//   #define PASSWD "your_password"

#endif // CONFIG_H