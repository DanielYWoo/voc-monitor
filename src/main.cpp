/**
 * VoC Monitor - Environmental Monitoring System
 * 
 * Reads from ENS160+AHT20 (chamber & room) sensors,
 * displays on ST7920 128x64 LCD.
 * 
 * Platform: ESP8266 (NodeMCU v3)
 * 
 * ST7920 LCD Wiring (Software SPI Mode):
 *   VCC  -> VIN (5V from USB)
 *   GND  -> GND
 *   V0   -> Leave floating (or 10k pot)
 *   RS   -> D8 (GPIO 15) - CS
 *   R/W  -> D7 (GPIO 13) - Data
 *   E    -> D0 (GPIO 16) - Clock
 *   PSB  -> GND (serial mode)
 *   RST  -> D4 (GPIO 2) - Reset
 *   BLA  -> 47Ω resistor -> RX (GPIO 3) - Backlight control
 *   BLK  -> GND
 * 
 * Button: D3 (GPIO 0) to GND
 * 
 * Screen Cycle (button press):
 *   1. SCREEN_MAIN      - Sensor data, backlight ON
 *   2. SCREEN_STATUS    - WiFi/Klipper status, backlight ON
 *   3. SCREEN_MAIN_DARK - Sensor data, backlight OFF
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SparkFun_ENS160.h>
#include <Adafruit_AHTX0.h>
#include "config.h"

#if WIFI_ENABLED
#include <ESP8266WiFi.h>
#include "credentials.h"
#if KLIPPER_PUSH_ENABLED
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#endif
#endif

// =============================================================================
// Hardware Setup
// =============================================================================
// ST7920 in serial mode - Software SPI
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD_CLK, LCD_DATA, LCD_CS, LCD_RST);

// =============================================================================
// I2C Buses and Sensors
// =============================================================================
// Chamber sensors use hardware I2C (Wire)
// Room sensors use software I2C - we'll use Wire with different pins
// Note: ESP8266 Wire library allows changing pins with Wire.begin(SDA, SCL)

SparkFun_ENS160 chamberENS;
Adafruit_AHTX0 chamberAHT;
SparkFun_ENS160 roomENS;
Adafruit_AHTX0 roomAHT;

// Software I2C for room sensors (using bit-banging)
// We'll implement a simple I2C scan approach

// =============================================================================
// Sensor Data
// =============================================================================
struct SensorData {
    // Chamber sensors (ENS160+AHT20)
    int chamberTVOC;      // ppb
    int chamberECO2;      // ppm
    int chamberTemp;      // °C (integer)
    int chamberHumidity;  // %
    bool chamberValid;    // true if sensors are connected
    
    // Room sensors (ENS160+AHT20)
    int roomTVOC;         // ppb
    int roomECO2;         // ppm
    int roomTemp;         // °C (integer)
    int roomHumidity;     // %
    bool roomValid;       // true if sensors are connected
};

SensorData data = {
    // Chamber (placeholder values)
    .chamberTVOC = 0,
    .chamberECO2 = 400,
    .chamberTemp = 25,
    .chamberHumidity = 50,
    .chamberValid = false,
    // Room (placeholder values)
    .roomTVOC = 0,
    .roomECO2 = 400,
    .roomTemp = 25,
    .roomHumidity = 50,
    .roomValid = false
};

// =============================================================================
// State
// =============================================================================
ScreenState currentScreen = SCREEN_MAIN;
volatile bool screenChanged = false;
volatile bool buttonPressed = false;  // Debug flag

// =============================================================================
// Button ISR
// =============================================================================
void IRAM_ATTR buttonISR() {
    static unsigned long lastPress = 0;
    unsigned long now = millis();
    if (now - lastPress > BUTTON_DEBOUNCE_MS) {
        lastPress = now;
        // Cycle: MAIN -> STATUS -> MAIN_DARK -> MAIN...
        currentScreen = (ScreenState)((currentScreen + 1) % 3);
        screenChanged = true;
        buttonPressed = true;  // Set debug flag
    }
}

// =============================================================================
// Backlight Control
// =============================================================================
void setBacklight(bool on) {
    digitalWrite(LCD_BACKLIGHT, on ? HIGH : LOW);
}

// =============================================================================
// Read All Sensors
// =============================================================================
void readSensors() {
    // Read Chamber sensors (on hardware I2C - Wire)
    // Switch to chamber I2C bus
    Wire.begin(I2C1_SDA, I2C1_SCL);
    delay(10);
    
    // Check if chamber ENS160 is connected
    data.chamberValid = chamberENS.isConnected();
    
    if (data.chamberValid) {
        data.chamberTVOC = chamberENS.getTVOC();
        data.chamberECO2 = chamberENS.getECO2();
        
        // Read AHT20 for temperature and humidity
        sensors_event_t humidity, temp;
        if (chamberAHT.getEvent(&humidity, &temp)) {
            data.chamberTemp = (int)temp.temperature;
            data.chamberHumidity = (int)humidity.relative_humidity;
            
            // Provide temperature compensation to ENS160
            chamberENS.setTempCompensationCelsius(temp.temperature);
            chamberENS.setRHCompensationFloat(humidity.relative_humidity);
        }
        
        Serial.printf("Chamber: TVOC=%dppb eCO2=%dppm T=%dC RH=%d%%\n",
                      data.chamberTVOC, data.chamberECO2, data.chamberTemp, data.chamberHumidity);
    } else {
        //Serial.println("Chamber: DISCONNECTED");
    }
    
    // Read Room sensors (on software I2C - different pins)
    // Switch to room I2C bus
    Wire.begin(I2C2_SDA, I2C2_SCL);
    delay(10);
    
    // Check if room ENS160 is connected
    data.roomValid = roomENS.isConnected();
    
    if (data.roomValid) {
        data.roomTVOC = roomENS.getTVOC();
        data.roomECO2 = roomENS.getECO2();
        
        // Read AHT20 for temperature and humidity
        sensors_event_t humidity, temp;
        if (roomAHT.getEvent(&humidity, &temp)) {
            data.roomTemp = (int)temp.temperature;
            data.roomHumidity = (int)humidity.relative_humidity;
            
            // Provide temperature compensation to ENS160
            roomENS.setTempCompensationCelsius(temp.temperature);
            roomENS.setRHCompensationFloat(humidity.relative_humidity);
        }
        
        Serial.printf("Room: TVOC=%dppb eCO2=%dppm T=%dC RH=%d%%\n",
                      data.roomTVOC, data.roomECO2, data.roomTemp, data.roomHumidity);
    } else {
        //Serial.println("Room: DISCONNECTED");
    }
}

// =============================================================================
// WiFi Functions
// =============================================================================
#if WIFI_ENABLED
bool wifiConnected = false;

void connectWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", SSID);
    WiFi.begin(SSID, PASSWD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed!");
    }
}

#if KLIPPER_PUSH_ENABLED
unsigned long lastKlipperPush = 0;
int lastPushResult = 0;

void pushToKlipper() {
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) return;
    
    WiFiClient client;
    HTTPClient http;
    String url = String("http://") + KLIPPER_HOST + ":" + KLIPPER_PORT + "/server/database/item";
    
    String payload = "{\"namespace\":\"voc_monitor\",\"key\":\"sensor_data\",\"value\":{";
    payload += "\"chamber_tvoc\":" + String(data.chamberTVOC) + ",";
    payload += "\"chamber_eco2\":" + String(data.chamberECO2) + ",";
    payload += "\"chamber_temp\":" + String(data.chamberTemp) + ",";
    payload += "\"chamber_rh\":" + String(data.chamberHumidity) + ",";
    payload += "\"room_tvoc\":" + String(data.roomTVOC) + ",";
    payload += "\"room_eco2\":" + String(data.roomECO2) + ",";
    payload += "\"room_temp\":" + String(data.roomTemp) + ",";
    payload += "\"room_rh\":" + String(data.roomHumidity) + ",";
    payload += "\"timestamp\":" + String(millis());
    payload += "}}";
    
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(payload);
    lastPushResult = httpCode;
    if (httpCode > 0) {
        Serial.printf("Klipper push: %d\n", httpCode);
    } else {
        Serial.printf("Klipper push failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}
#endif
#endif

// =============================================================================
// Display Functions
// =============================================================================
void drawMainScreen() {
    // Two-column layout with table borders
    // Using 5x8 font: 25 chars per line, 8 lines
    // Each column: 64 pixels wide
    char buf[16];
    int textWidth;
    u8g2.setFont(u8g2_font_5x8_tf);
    
    // Draw outer frame
    u8g2.drawFrame(0, 0, 128, 64);
    
    // Draw vertical divider (center)
    u8g2.drawVLine(64, 0, 44);
    
    // Draw horizontal lines
    u8g2.drawHLine(0, 10, 128);   // Below header
    u8g2.drawHLine(0, 44, 128);   // Above WiFi status
    
    // Header row
    u8g2.drawStr(14, 8, "CHAMBER");
    u8g2.drawStr(82, 8, "ROOM");
    
    // Chamber column (left) - right aligned values
    if (data.chamberValid) {
        u8g2.drawStr(2, 20, "TVOC:");
        snprintf(buf, sizeof(buf), "%dppb", data.chamberTVOC);
        textWidth = u8g2.getStrWidth(buf);
        u8g2.drawStr(62 - textWidth, 20, buf);
        
        u8g2.drawStr(2, 30, "eCO2:");
        snprintf(buf, sizeof(buf), "%dppm", data.chamberECO2);
        textWidth = u8g2.getStrWidth(buf);
        u8g2.drawStr(62 - textWidth, 30, buf);
        
        snprintf(buf, sizeof(buf), "T:%dC RH:%d%%", data.chamberTemp, data.chamberHumidity);
        u8g2.drawStr(2, 40, buf);
    } else {
        u8g2.drawStr(2, 20, "TVOC:");
        u8g2.drawStr(42, 20, "ERR");
        u8g2.drawStr(2, 30, "eCO2:");
        u8g2.drawStr(42, 30, "ERR");
        u8g2.drawStr(2, 40, "DISCONNECTED");
    }
    
    // Room column (right) - right aligned values
    if (data.roomValid) {
        u8g2.drawStr(66, 20, "TVOC:");
        snprintf(buf, sizeof(buf), "%dppb", data.roomTVOC);
        textWidth = u8g2.getStrWidth(buf);
        u8g2.drawStr(126 - textWidth, 20, buf);
        
        u8g2.drawStr(66, 30, "eCO2:");
        snprintf(buf, sizeof(buf), "%dppm", data.roomECO2);
        textWidth = u8g2.getStrWidth(buf);
        u8g2.drawStr(126 - textWidth, 30, buf);
        
        snprintf(buf, sizeof(buf), "T:%dC RH:%d%%", data.roomTemp, data.roomHumidity);
        u8g2.drawStr(66, 40, buf);
    } else {
        u8g2.drawStr(66, 20, "TVOC:");
        u8g2.drawStr(106, 20, "ERR");
        u8g2.drawStr(66, 30, "eCO2:");
        u8g2.drawStr(106, 30, "ERR");
        u8g2.drawStr(66, 40, "DISCONNECTED");
    }
    
    // Bottom rows: WiFi status (full width)
    #if WIFI_ENABLED
    if (wifiConnected) {
        u8g2.drawStr(2, 52, "WiFi: Connected");
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        u8g2.drawStr(2, 62, buf);
    } else {
        u8g2.drawStr(2, 52, "WiFi: Disconnected");
        u8g2.drawStr(2, 62, "IP: ---.---.---.---");
    }
    #else
    u8g2.drawStr(2, 52, "WiFi: Disabled");
    u8g2.drawStr(2, 62, "IP: N/A");
    #endif
}

void drawStatusScreen() {
    // Status screen with Klipper info
    u8g2.setFont(u8g2_font_5x8_tf);
    
    // Draw outer frame
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawHLine(0, 10, 128);
    
    // Header
    u8g2.drawStr(45, 8, "STATUS");
    
    // WiFi status
    #if WIFI_ENABLED
    char buf[26];
    snprintf(buf, sizeof(buf), "WiFi: %s", wifiConnected ? "Connected" : "Disconnected");
    u8g2.drawStr(2, 20, buf);
    
    if (wifiConnected) {
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(buf, sizeof(buf), "IP: ---.---.---.---");
    }
    u8g2.drawStr(2, 30, buf);
    
    // Klipper Push status
    #if KLIPPER_PUSH_ENABLED
    u8g2.drawStr(2, 40, "Klipper Push: ON");
    snprintf(buf, sizeof(buf), "Host: %s:%d", KLIPPER_HOST, KLIPPER_PORT);
    u8g2.drawStr(2, 50, buf);
    
    if (lastPushResult == 0) {
        u8g2.drawStr(2, 60, "Last Push: --");
    } else if (lastPushResult == 200) {
        u8g2.drawStr(2, 60, "Last Push: OK");
    } else {
        snprintf(buf, sizeof(buf), "Last Push: ERR %d", lastPushResult);
        u8g2.drawStr(2, 60, buf);
    }
    #else
    u8g2.drawStr(2, 40, "Klipper Push: OFF");
    u8g2.drawStr(2, 50, "Host: N/A");
    u8g2.drawStr(2, 60, "Last Push: N/A");
    #endif
    #else
    u8g2.drawStr(2, 20, "WiFi: Disabled");
    u8g2.drawStr(2, 30, "IP: N/A");
    u8g2.drawStr(2, 40, "Klipper Push: OFF");
    u8g2.drawStr(2, 50, "Host: N/A");
    u8g2.drawStr(2, 60, "Last Push: N/A");
    #endif
}

void updateDisplay() {
    u8g2.clearBuffer();
    switch (currentScreen) {
        case SCREEN_MAIN:
            drawMainScreen();
            setBacklight(true);
            break;
        case SCREEN_STATUS:
            drawStatusScreen();
            setBacklight(true);
            break;
        case SCREEN_MAIN_DARK:
            drawMainScreen();
            setBacklight(false);
            break;
    }
    u8g2.sendBuffer();
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== VoC Monitor (ESP8266) ===\n");
    
    // Print pin configuration
    Serial.println("LCD Pins: CLK=D0(16), DATA=D7(13), CS=D8(15), RST=D4(2)");
    Serial.println("I2C1: SDA=D2(4), SCL=D1(5)");
    Serial.println("I2C2: SDA=D6(12), SCL=D5(14)");
    Serial.println("Button: D3(0)");
    
    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    // Backlight control
    pinMode(LCD_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_BACKLIGHT, HIGH);  // Start with backlight on
    
    // LCD initialization
    Serial.println("Initializing LCD...");
    delay(100);
    
    u8g2.begin();
    delay(100);
    u8g2.setPowerSave(0);
    
    // Initialize Chamber sensors (I2C1)
    Serial.println("Initializing Chamber sensors...");
    Wire.begin(I2C1_SDA, I2C1_SCL);
    delay(100);
    
    if (chamberENS.begin(Wire, ENS160_ADDR)) {
        Serial.println("Chamber ENS160: OK");
        chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.chamberValid = true;
    } else {
        Serial.println("Chamber ENS160: NOT FOUND");
        data.chamberValid = false;
    }
    
    if (chamberAHT.begin(&Wire, 0, AHT20_ADDR)) {
        Serial.println("Chamber AHT20: OK");
    } else {
        Serial.println("Chamber AHT20: NOT FOUND");
    }
    
    // Initialize Room sensors (I2C2)
    Serial.println("Initializing Room sensors...");
    Wire.begin(I2C2_SDA, I2C2_SCL);
    delay(100);
    
    if (roomENS.begin(Wire, ENS160_ADDR)) {
        Serial.println("Room ENS160: OK");
        roomENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.roomValid = true;
    } else {
        Serial.println("Room ENS160: NOT FOUND");
        data.roomValid = false;
    }
    
    if (roomAHT.begin(&Wire, 0, AHT20_ADDR)) {
        Serial.println("Room AHT20: OK");
    } else {
        Serial.println("Room AHT20: NOT FOUND");
    }
    
    // WiFi
    #if WIFI_ENABLED
    connectWiFi();
    #endif
    
    // Initial read
    delay(1000);
    readSensors();
    updateDisplay();
    
    Serial.println("Ready. Press button to cycle screens.");
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    static unsigned long lastRead = 0;
    
    if (millis() - lastRead >= SENSOR_READ_INTERVAL) {
        lastRead = millis();
        readSensors();
        updateDisplay();
    }
    
    #if WIFI_ENABLED && KLIPPER_PUSH_ENABLED
    if (millis() - lastKlipperPush >= KLIPPER_PUSH_INTERVAL) {
        lastKlipperPush = millis();
        pushToKlipper();
    }
    #endif
    
    if (screenChanged) {
        screenChanged = false;
        updateDisplay();
    }
    
    // Debug: print button press
    if (buttonPressed) {
        buttonPressed = false;
        Serial.printf("Button pressed! Screen: %d\n", currentScreen);
    }
    
    delay(50);
}