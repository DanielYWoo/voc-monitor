/**
 * VoC Monitor - Real Sensor Demo
 * 
 * Reads from DHT11 (temperature/humidity) and MQ135 (VOC) sensors,
 * displays on ST7920 128x64 LCD.
 * 
 * ST7920 LCD Wiring (Hardware SPI Mode):
 *   VCC  -> 5V (pin 2)
 *   GND  -> GND (pin 1)
 *   V0   -> GND (pin 3) - max contrast
 *   RS   -> GPIO 5 (CS) (pin 4)
 *   R/W  -> GPIO 23 (MOSI) (pin 5)
 *   E    -> GPIO 18 (CLK) (pin 6)
 *   PSB  -> GND (serial mode) (pin 15)
 *   RST  -> 5V (pin 17) - tied high, no GPIO needed
 *   BLA  -> 3.3V (backlight) (pin 19)
 *   BLK  -> GND (pin 20)
 * 
 * Based on: https://www.instructables.com/ST7920-128X64-LCD-Display-to-ESP32/
 * 
 * DHT11: VCC -> 3.3V, DATA -> GPIO 33, GND -> GND
 * MQ135: VCC -> 5V, AO -> voltage divider -> GPIO 32, GND -> GND
 * Button: GPIO 26 to GND
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <Preferences.h>
#include "config.h"

#if WIFI_ENABLED
#include <WiFi.h>
#include "credentials.h"
#if KLIPPER_PUSH_ENABLED
#include <HTTPClient.h>
#endif
#endif

// =============================================================================
// Hardware Setup
// =============================================================================
// ST7920 in serial mode
// 
// Based on: https://www.instructables.com/ST7920-128X64-LCD-Display-to-ESP32/
// The article uses Hardware SPI with these pins:
//   LCD RS (pin 4)  → GPIO 5  (SPI CS)
//   LCD R/W (pin 5) → GPIO 23 (SPI MOSI) 
//   LCD E (pin 6)   → GPIO 18 (SPI CLK) - VSPI hardware clock
//   LCD RST (pin 17)→ 5V (tied high, no GPIO needed)
//
// IMPORTANT: Use Hardware SPI constructor, not Software SPI!
// The ESP32 VSPI pins are: CLK=18, MOSI=23, CS=5
//
// Hardware SPI constructor (recommended for ESP32):
// U8X8_PIN_NONE means RST is tied to VCC (no software reset needed)
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, /* cs=RS */ 5, /* reset */ U8X8_PIN_NONE);

// If hardware SPI doesn't work, try software SPI with GPIO 18:
// U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=E */ 18, /* data=R/W */ 23, /* cs=RS */ 5, /* reset */ 22);

DHT dht(DHT11_PIN, DHT11);
Preferences prefs;

// =============================================================================
// Sensor Data
// =============================================================================
struct SensorData {
    float temperature;    // °C
    float humidity;       // %
    bool dhtValid;        // true if reading is valid
    float toluenePPM;     // Estimated Toluene PPM
    int rawADC;           // Raw ADC for debugging
    float mq135Ro;        // Baseline resistance in clean air (kΩ)
    bool calibrated;      // true if user calibrated
};

SensorData data = {
    .temperature = 25.0,
    .humidity = 50.0,
    .dhtValid = false,
    .toluenePPM = 0,
    .rawADC = 0,
    .mq135Ro = MQ135_RO_CLEAN_AIR,
    .calibrated = false
};

// =============================================================================
// State
// =============================================================================
ScreenState currentScreen = SCREEN_MAIN;
volatile bool screenChanged = false;

// =============================================================================
// Button ISR
// =============================================================================
void IRAM_ATTR buttonISR() {
    static unsigned long lastPress = 0;
    unsigned long now = millis();
    if (now - lastPress > BUTTON_DEBOUNCE_MS) {
        lastPress = now;
        currentScreen = (ScreenState)((currentScreen + 1) % 3);
        screenChanged = true;
    }
}

// =============================================================================
// MQ135 Reading with Temperature/Humidity Compensation
// =============================================================================
float readMQ135(float temp, float humidity, bool tempValid) {
    int raw = analogRead(MQ135_PIN);
    data.rawADC = raw;
    
    float voltage = raw * (3.3f / 4095.0f);
    if (voltage < 0.01f) voltage = 0.01f;
    
    // Reconstruct original MQ135 voltage (before 10k+20k divider)
    float mq135Voltage = voltage * 1.5f;
    if (mq135Voltage > 4.9f) mq135Voltage = 4.9f;
    
    // Calculate sensor resistance: Rs = RL * (Vc - Vout) / Vout
    float rs = MQ135_RL * (5.0f - mq135Voltage) / mq135Voltage;
    float rsRo = rs / data.mq135Ro;
    
    // Apply temperature/humidity correction if available
    if (tempValid) {
        float correction = MQ135_CORA * temp * temp 
                         + MQ135_CORB * temp 
                         + MQ135_CORC 
                         - (humidity - 33.0f) * MQ135_CORD;
        rsRo = rsRo / correction;
    }
    
    // Calculate Toluene PPM: PPM = a * (Rs/Ro)^b
    float ppm = MQ135_TOLUENE_A * powf(rsRo, MQ135_TOLUENE_B);
    return constrain(ppm, 0.1f, 999.0f);
}

// =============================================================================
// Read All Sensors
// =============================================================================
void readSensors() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (isnan(t) || isnan(h)) {
        data.dhtValid = false;
    } else {
        data.temperature = t;
        data.humidity = h;
        data.dhtValid = true;
    }
    
    data.toluenePPM = readMQ135(data.temperature, data.humidity, data.dhtValid);
    
    Serial.printf("DHT11: %.1f°C %.1f%% (%s) | MQ135: %.1fppm (raw:%d)\n",
                  data.temperature, data.humidity, 
                  data.dhtValid ? "OK" : "ERR",
                  data.toluenePPM, data.rawADC);
}

// =============================================================================
// MQ135 Calibration
// =============================================================================
void calibrateMQ135() {
    Serial.println("Calibrating MQ135 in clean air...");
    
    float rsSum = 0;
    int count = 0;
    
    for (int i = 0; i < 50; i++) {
        int raw = analogRead(MQ135_PIN);
        float voltage = raw * (3.3f / 4095.0f);
        if (voltage > 0.01f) {
            float mq135Voltage = voltage * 1.5f;
            if (mq135Voltage < 4.9f) {
                float rs = MQ135_RL * (5.0f - mq135Voltage) / mq135Voltage;
                rsSum += rs;
                count++;
            }
        }
        delay(100);
    }
    
    if (count > 0) {
        data.mq135Ro = rsSum / count;
        data.calibrated = true;
        
        prefs.begin("voc", false);
        prefs.putFloat("mq135Ro", data.mq135Ro);
        prefs.putBool("calibrated", true);
        prefs.end();
        
        Serial.printf("Calibration done. Ro = %.2f kΩ\n", data.mq135Ro);
    } else {
        Serial.println("Calibration failed!");
    }
}

void loadCalibration() {
    prefs.begin("voc", true);
    if (prefs.isKey("mq135Ro")) {
        data.mq135Ro = prefs.getFloat("mq135Ro", MQ135_RO_CLEAN_AIR);
        data.calibrated = prefs.getBool("calibrated", false);
        Serial.printf("Loaded calibration: Ro = %.2f kΩ\n", data.mq135Ro);
    }
    prefs.end();
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

void pushToKlipper() {
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    String url = String("http://") + KLIPPER_HOST + ":" + KLIPPER_PORT + "/server/database/item";
    
    String payload = "{\"namespace\":\"voc_monitor\",\"key\":\"sensor_data\",\"value\":{";
    payload += "\"temperature\":" + String(data.temperature, 1) + ",";
    payload += "\"humidity\":" + String(data.humidity, 1) + ",";
    payload += "\"toluene_ppm\":" + String(data.toluenePPM, 1) + ",";
    payload += "\"dht_valid\":" + String(data.dhtValid ? "true" : "false") + ",";
    payload += "\"calibrated\":" + String(data.calibrated ? "true" : "false") + ",";
    payload += "\"timestamp\":" + String(millis());
    payload += "}}";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(payload);
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
    char buf[32];
    u8g2.setFont(u8g2_font_4x6_tf);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawStr(35, 7, "VOC MONITOR");
    u8g2.drawHLine(0, 9, 128);
    
    // DHT11
    u8g2.drawStr(2, 17, "DHT11:");
    snprintf(buf, sizeof(buf), "T:%.1fC RH:%.1f%% %s", 
             data.temperature, data.humidity, data.dhtValid ? "" : "(ERR)");
    u8g2.drawStr(2, 25, buf);
    
    // MQ135
    u8g2.drawHLine(0, 27, 128);
    u8g2.drawStr(2, 35, "MQ135 Toluene:");
    
    // Bar
    int barWidth = map(constrain((int)data.toluenePPM, 0, 100), 0, 100, 0, 124);
    u8g2.drawFrame(2, 37, 124, 10);
    if (barWidth > 0) u8g2.drawBox(2, 37, barWidth, 10);
    
    snprintf(buf, sizeof(buf), "%.1f ppm", data.toluenePPM);
    u8g2.setFontMode(1);
    u8g2.setDrawColor(2);
    u8g2.drawStr(50, 45, buf);
    u8g2.setDrawColor(1);
    u8g2.setFontMode(0);
    
    // Status
    snprintf(buf, sizeof(buf), "Ro:%.1fk %s | Raw:%d", 
             data.mq135Ro, data.calibrated ? "CAL" : "DEF", data.rawADC);
    u8g2.drawStr(2, 61, buf);
}

void drawStatusScreen() {
    char buf[32];
    u8g2.setFont(u8g2_font_4x6_tf);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawStr(2, 7, "STATUS");
    u8g2.drawHLine(0, 9, 128);
    
    snprintf(buf, sizeof(buf), "DHT11: %s", data.dhtValid ? "OK" : "ERROR");
    u8g2.drawStr(2, 17, buf);
    
    snprintf(buf, sizeof(buf), "MQ135 Ro: %.2f kOhm", data.mq135Ro);
    u8g2.drawStr(2, 25, buf);
    
    snprintf(buf, sizeof(buf), "Calibrated: %s", data.calibrated ? "Yes" : "No");
    u8g2.drawStr(2, 33, buf);
    
    snprintf(buf, sizeof(buf), "Heap: %d bytes", ESP.getFreeHeap());
    u8g2.drawStr(2, 41, buf);
    
    unsigned long s = millis() / 1000;
    snprintf(buf, sizeof(buf), "Uptime: %lum %lus", s / 60, s % 60);
    u8g2.drawStr(2, 49, buf);
    
    u8g2.drawStr(2, 61, "BTN: cycle screens");
}

void drawScreenOff() {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(40, 32, "Press BTN");
}

void updateDisplay() {
    u8g2.clearBuffer();
    switch (currentScreen) {
        case SCREEN_OFF:    drawScreenOff(); break;
        case SCREEN_MAIN:   drawMainScreen(); break;
        case SCREEN_STATUS: drawStatusScreen(); break;
    }
    u8g2.sendBuffer();
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== VoC Monitor ===\n");
    
    // Print pin configuration
    Serial.println("LCD Pins: E(Clock)=GPIO2, R/W(Data)=GPIO23, RS(CS)=GPIO5");
    
    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    // DHT11
    dht.begin();
    
    // MQ135 ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    // Load or run calibration
    #ifdef CALIBRATION_MODE
    delay(2000);
    calibrateMQ135();
    #else
    loadCalibration();
    #endif
    
    // LCD - ST7920 needs time to power up (40ms minimum per datasheet)
    Serial.println("Initializing LCD...");
    delay(100);  // Wait for LCD power stabilization
    
    u8g2.begin();
    Serial.println("u8g2.begin() done");
    
    delay(100);  // Additional delay after init
    u8g2.setPowerSave(0);
    Serial.println("Power save off");
    
    // Draw test pattern
    Serial.println("Drawing test pattern...");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Hello World!");
    u8g2.drawBox(0, 20, 50, 10);
    u8g2.drawFrame(60, 20, 50, 10);
    u8g2.sendBuffer();
    Serial.println("Test pattern sent");
    
    delay(3000);  // Show test pattern for 3 seconds
    
    // WiFi
    #if WIFI_ENABLED
    connectWiFi();
    #endif
    
    // Initial read
    delay(1000);
    readSensors();
    updateDisplay();
    
    Serial.println("Ready.");
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
    
    delay(50);
}