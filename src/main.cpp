/**
 * VoC Monitor - Real Sensor Demo
 * 
 * Reads from DHT11 (temperature/humidity) and MQ135 (VOC) sensors,
 * displays on ST7920 128x64 LCD.
 * 
 * ST7920 LCD Wiring (Serial/SPI Mode):
 *   VCC  -> 5V, GND -> GND
 *   RS   -> GPIO 5 (CS), R/W -> GPIO 23 (Data), E -> GPIO 18 (Clock)
 *   PSB  -> GND (serial mode), RST -> 5V
 *   BLA  -> 5V via 100Ω, BLK -> GND
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
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD_SCK, LCD_MOSI, LCD_CS, U8X8_PIN_NONE);
DHT dht(DHT11_PIN, DHT11);
Preferences prefs;

// =============================================================================
// Sensor Data - Only raw readings and final outputs
// =============================================================================
struct SensorData {
    // DHT11 readings
    float temperature;    // °C
    float humidity;       // %
    bool dhtValid;        // true if reading is valid
    
    // MQ135 output
    float toluenePPM;     // Estimated Toluene PPM
    int rawADC;           // Raw ADC for debugging
    
    // Calibration (stored in NVS)
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
// Button ISR - Only sets flag, no debounce logic in ISR
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
    // Read raw ADC
    int raw = analogRead(MQ135_PIN);
    data.rawADC = raw;
    
    // Convert to voltage (ESP32 ADC: 12-bit, 0-3.3V)
    float voltage = raw * (3.3f / 4095.0f);
    if (voltage < 0.01f) voltage = 0.01f;
    
    // Reconstruct original MQ135 voltage (before 10k+20k divider)
    // Vout = Vin * 20k / (10k + 20k) => Vin = Vout * 1.5
    float mq135Voltage = voltage * 1.5f;
    if (mq135Voltage > 4.9f) mq135Voltage = 4.9f;
    
    // Calculate sensor resistance: Rs = RL * (Vc - Vout) / Vout
    float rs = MQ135_RL * (5.0f - mq135Voltage) / mq135Voltage;
    
    // Calculate Rs/Ro ratio
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
    
    // Clamp to reasonable range
    return constrain(ppm, 0.1f, 999.0f);
}

// =============================================================================
// Read All Sensors
// =============================================================================
void readSensors() {
    // Read DHT11
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (isnan(t) || isnan(h)) {
        data.dhtValid = false;
    } else {
        data.temperature = t;
        data.humidity = h;
        data.dhtValid = true;
    }
    
    // Read MQ135 with compensation
    data.toluenePPM = readMQ135(data.temperature, data.humidity, data.dhtValid);
    
    // Debug output
    Serial.printf("DHT11: %.1f°C %.1f%% (%s) | MQ135: %.1fppm (raw:%d)\n",
                  data.temperature, data.humidity, 
                  data.dhtValid ? "OK" : "ERR",
                  data.toluenePPM, data.rawADC);
}

// =============================================================================
// MQ135 Calibration - Call in clean air after 24h burn-in
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
        
        // Save to NVS
        prefs.begin("voc", false);
        prefs.putFloat("mq135Ro", data.mq135Ro);
        prefs.putBool("calibrated", true);
        prefs.end();
        
        Serial.printf("Calibration done. Ro = %.2f kΩ (saved to NVS)\n", data.mq135Ro);
    } else {
        Serial.println("Calibration failed!");
    }
}

void loadCalibration() {
    prefs.begin("voc", true);  // Read-only
    if (prefs.isKey("mq135Ro")) {
        data.mq135Ro = prefs.getFloat("mq135Ro", MQ135_RO_CLEAN_AIR);
        data.calibrated = prefs.getBool("calibrated", false);
        Serial.printf("Loaded calibration: Ro = %.2f kΩ\n", data.mq135Ro);
    }
    prefs.end();
}

// =============================================================================
// WiFi Functions (only compiled if WIFI_ENABLED)
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
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    HTTPClient http;
    String url = String("http://") + KLIPPER_HOST + ":" + KLIPPER_PORT + "/server/database/item";
    
    // Build JSON payload
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
#endif // KLIPPER_PUSH_ENABLED
#endif // WIFI_ENABLED

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
    
    // Button with interrupt
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    // DHT11
    dht.begin();
    
    // MQ135 ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    // Load calibration from NVS (or run calibration if CALIBRATION_MODE is defined)
    #ifdef CALIBRATION_MODE
    Serial.println("*** CALIBRATION MODE - Running calibration on boot ***");
    delay(2000);  // Give sensor time to warm up
    calibrateMQ135();
    #else
    loadCalibration();
    #endif
    
    // Display
    u8g2.begin();
    u8g2.setContrast(255);
    
    // WiFi (if enabled)
    #if WIFI_ENABLED
    connectWiFi();
    #endif
    
    // Initial read
    delay(2000);
    readSensors();
    updateDisplay();
    
    Serial.println("Ready. Press button to cycle screens.");
    Serial.println("To calibrate: uncomment CALIBRATION_MODE in config.h and re-upload.\n");
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    static unsigned long lastRead = 0;
    
    // Read sensors periodically
    if (millis() - lastRead >= SENSOR_READ_INTERVAL) {
        lastRead = millis();
        readSensors();
        updateDisplay();
    }
    
    // Push to Klipper periodically (if enabled)
    #if WIFI_ENABLED && KLIPPER_PUSH_ENABLED
    if (millis() - lastKlipperPush >= KLIPPER_PUSH_INTERVAL) {
        lastKlipperPush = millis();
        pushToKlipper();
    }
    #endif
    
    // Update display on screen change
    if (screenChanged) {
        screenChanged = false;
        updateDisplay();
    }
    
    // Light sleep for power saving (optional - comment out if not needed)
    // esp_sleep_enable_timer_wakeup(100 * 1000);  // 100ms
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_26, 0);
    // esp_light_sleep_start();
    
    delay(50);  // Simple delay instead of busy loop
}
