/**
 * VOC Monitor - Environmental Monitoring System
 * 
 * Reads from ENS160+AHT20 (chamber & room) sensors,
 * displays on ST7920 128x64 LCD, serves web dashboard.
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
 * Button: TX (GPIO 1) - Digital input with interrupt (Serial TX disabled)
 * Buzzer: D8 (GPIO 15) - Passive buzzer
 * 
 * Screen Cycle (button press):
 *   1. SCREEN_MAIN      - Sensor data, backlight ON
 *   2. SCREEN_STATUS    - WiFi/Web status, backlight ON
 *   3. SCREEN_MAIN_DARK - Sensor data, backlight OFF
 * 
 * Web Dashboard:
 *   http://<device-ip>/         - Dashboard with Chart.js graphs
 *   http://<device-ip>/api/current - Current sensor readings (JSON)
 *   http://<device-ip>/api/data    - Historical data (JSON)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SparkFun_ENS160.h>
#include <Adafruit_AHTX0.h>
#include "config.h"

#if WIFI_ENABLED
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include "web_content.h"
#endif

// =============================================================================
// Hardware Setup
// =============================================================================
// ST7920 in serial mode - Software SPI
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD_CLK, LCD_DATA, LCD_CS, LCD_RST);

// =============================================================================
// I2C Buses and Sensors
// =============================================================================
SparkFun_ENS160 chamberENS;
Adafruit_AHTX0 chamberAHT;
SparkFun_ENS160 roomENS;
Adafruit_AHTX0 roomAHT;

// =============================================================================
// Sensor Data (Current Reading)
// =============================================================================
struct SensorData {
    int chamberTVOC;      // ppb
    int chamberECO2;      // ppm
    int chamberTemp;      // °C (integer)
    int chamberHumidity;  // %
    bool chamberValid;
    
    int roomTVOC;         // ppb
    int roomECO2;         // ppm
    int roomTemp;         // °C (integer)
    int roomHumidity;     // %
    bool roomValid;
};

SensorData data = {
    .chamberTVOC = -1, .chamberECO2 = -1, .chamberTemp = -1, .chamberHumidity = -1, .chamberValid = false,
    .roomTVOC = -1, .roomECO2 = -1, .roomTemp = -1, .roomHumidity = -1, .roomValid = false
};

// =============================================================================
// Historical Data Buffer (30 minutes of 1-minute averages)
// =============================================================================
#define HISTORY_SIZE 30
#define SAMPLES_PER_MINUTE 60

// 1-minute averaged data point (16 bytes)
struct HistoryPoint {
    uint16_t chamberTVOC;
    uint16_t chamberECO2;
    int16_t  chamberTemp;
    uint16_t chamberRH;
    uint16_t roomTVOC;
    uint16_t roomECO2;
    int16_t  roomTemp;
    uint16_t roomRH;
};

HistoryPoint history[HISTORY_SIZE];  // Circular buffer (480 bytes)
unsigned long historyPointCount = 0; // Total points stored (used for write index and valid count)

// Running average accumulators (double for precision)
double avgChamberTVOC = 0;
double avgChamberECO2 = 0;
double avgChamberTemp = 0;
double avgChamberRH = 0;
double avgRoomTVOC = 0;
double avgRoomECO2 = 0;
double avgRoomTemp = 0;
double avgRoomRH = 0;
uint8_t sampleCount = 0;

// =============================================================================
// State
// =============================================================================
#define WARMUP_MINUTES 60  // ENS160 needs ~60 minutes to stabilize

ScreenState currentScreen = SCREEN_MAIN;
uint16_t sensorWarmupCountdown = WARMUP_MINUTES * 60;  // Countdown in seconds (0 = warmed up)

// =============================================================================
// Buzzer State (Geiger counter effect + Alarm)
// =============================================================================
unsigned long lastClickTime = 0;
unsigned long nextClickInterval = 0;
#define BUZZER_CLICK_FREQ    400    // Hz - click tone frequency (very deep sound)
#define BUZZER_CLICK_DURATION 3     // ms - very short click like Geiger counter
#define BUZZER_MIN_INTERVAL  50     // ms - fastest clicking (high radiation)
#define BUZZER_MAX_INTERVAL  2000   // ms - slowest clicking (threshold level)

// Alarm state (for sensor error)
#define BUZZER_ALARM_FREQ     800   // Hz - alarm tone frequency (lowered)
bool errorBlinkOn = false;  // Toggled every SENSOR_READ_INTERVAL when sensor error

// =============================================================================
// Forward Declarations
// =============================================================================
void drawBootScreen(const char* status, uint8_t progress = 0);
void scanI2C(int sda, int scl);
void updateBuzzer();

// =============================================================================
// Buzzer - Geiger Counter Effect
// =============================================================================
// Calculate click interval based on room sensor severity
// Higher readings = shorter interval = more frequent clicks
unsigned long calculateClickInterval() {
    if (!data.roomValid) return 0;  // No clicking if sensor invalid
    
    // Calculate severity ratio (0.0 = at threshold, 1.0 = at max)
    float tvocSeverity = 0;
    float eco2Severity = 0;
    
    if (data.roomTVOC > TVOC_WARNING) {
        tvocSeverity = (float)(data.roomTVOC - TVOC_WARNING) / (float)(TVOC_MAX - TVOC_WARNING);
        tvocSeverity = constrain(tvocSeverity, 0.0f, 1.0f);
    }
    
    if (data.roomECO2 > ECO2_WARNING) {
        eco2Severity = (float)(data.roomECO2 - ECO2_WARNING) / (float)(ECO2_MAX - ECO2_WARNING);
        eco2Severity = constrain(eco2Severity, 0.0f, 1.0f);
    }
    
    // Use the higher severity
    float severity = max(tvocSeverity, eco2Severity);
    
    if (severity <= 0) return 0;  // Below threshold, no clicking
    
    // Map severity to interval: high severity = short interval
    // Add randomness for authentic Geiger counter feel
    unsigned long baseInterval = BUZZER_MAX_INTERVAL - (unsigned long)(severity * (BUZZER_MAX_INTERVAL - BUZZER_MIN_INTERVAL));
    
    // Add ±30% randomness
    long randomOffset = (long)(baseInterval * 0.3) - random(0, (long)(baseInterval * 0.6));
    unsigned long interval = baseInterval + randomOffset;
    
    return constrain(interval, BUZZER_MIN_INTERVAL, BUZZER_MAX_INTERVAL);
}

// Update buzzer - handles both alarm mode and Geiger counter mode
void updateBuzzer() {
    // ALARM MODE: Either sensor error - beep pattern using tone with duration
    if (!data.roomValid || !data.chamberValid) {
        if (errorBlinkOn) {
            // Use tone with 100ms duration, refreshed every ~50ms loop
            // When errorBlinkOn becomes false, tone auto-stops within 100ms
            tone(BUZZER_PIN, BUZZER_ALARM_FREQ, 100);
        }
        return;
    }
    
    // GEIGER MODE: Random clicks based on room sensor readings
    unsigned long now = millis();
    if (now - lastClickTime >= nextClickInterval) {
        nextClickInterval = calculateClickInterval();
        if (nextClickInterval > 0) {
            tone(BUZZER_PIN, BUZZER_CLICK_FREQ, BUZZER_CLICK_DURATION);
        }
        lastClickTime = now;
    }
}

// =============================================================================
// WiFi and Web Server
// =============================================================================
#if WIFI_ENABLED
bool wifiConnected = false;
AsyncWebServer server(80);

void setupWebServer() {
    // Serve main HTML page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    
    // API: Current readings - streamed response
    server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        
        // Helper to write value or status string
        auto writeVal = [response](const char* key, int val, bool valid, bool isFirst = false) {
            if (!isFirst) response->print(",");
            response->printf("\"%s\":", key);
            if (!valid) {
                response->print("\"Error\"");
            } else if (val < 0) {
                response->print("\"N/A\"");
            } else {
                response->print(val);
            }
        };
        
        response->print("{");
        
        // Chamber values
        int chTvoc = sampleCount > 0 ? (int)avgChamberTVOC : data.chamberTVOC;
        int chEco2 = sampleCount > 0 ? (int)avgChamberECO2 : data.chamberECO2;
        int chTemp = sampleCount > 0 ? (int)avgChamberTemp : data.chamberTemp;
        int chRh = sampleCount > 0 ? (int)avgChamberRH : data.chamberHumidity;
        
        writeVal("ch_tvoc", chTvoc, data.chamberValid, true);
        writeVal("ch_eco2", chEco2, data.chamberValid);
        writeVal("ch_temp", chTemp, data.chamberValid);
        writeVal("ch_rh", chRh, data.chamberValid);
        
        // Room values
        int rmTvoc = sampleCount > 0 ? (int)avgRoomTVOC : data.roomTVOC;
        int rmEco2 = sampleCount > 0 ? (int)avgRoomECO2 : data.roomECO2;
        int rmTemp = sampleCount > 0 ? (int)avgRoomTemp : data.roomTemp;
        int rmRh = sampleCount > 0 ? (int)avgRoomRH : data.roomHumidity;
        
        writeVal("rm_tvoc", rmTvoc, data.roomValid);
        writeVal("rm_eco2", rmEco2, data.roomValid);
        writeVal("rm_temp", rmTemp, data.roomValid);
        writeVal("rm_rh", rmRh, data.roomValid);
        
        response->printf(",\"samples\":%d}", sampleCount);
        request->send(response);
    });
    
    // API: Historical data - streamed response
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        
        unsigned long validPoints = min(historyPointCount, (unsigned long)HISTORY_SIZE);
        response->print("[");
        for (unsigned long i = 0; i < validPoints; i++) {
            // Read from oldest to newest: start from (current - validPoints) position
            unsigned long idx = (historyPointCount - validPoints + i) % HISTORY_SIZE;
            if (i > 0) response->print(",");
            response->printf(
                "{\"ch_tvoc\":%u,\"ch_eco2\":%u,\"ch_temp\":%d,\"ch_rh\":%u,"
                "\"rm_tvoc\":%u,\"rm_eco2\":%u,\"rm_temp\":%d,\"rm_rh\":%u}",
                history[idx].chamberTVOC, history[idx].chamberECO2,
                history[idx].chamberTemp, history[idx].chamberRH,
                history[idx].roomTVOC, history[idx].roomECO2,
                history[idx].roomTemp, history[idx].roomRH
            );
        }
        response->print("]");
        request->send(response);
    });
    
    server.begin();
}

void connectWiFi() {
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    drawBootScreen("Connecting to WiFi...", 0);
    WiFi.begin(SSID, PASSWD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        drawBootScreen("Connecting to WiFi...", attempts + 1);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        drawBootScreen("WiFi connected!", 0);
        delay(500);
        setupWebServer();
    } else {
        wifiConnected = false;
        drawBootScreen("WiFi failed, continuing...", 0);
        delay(1000);
    }
}
#endif

// =============================================================================
// Button Interrupt (GPIO 1 / TX pin)
// =============================================================================
// Button uses interrupt on FALLING edge (press detection)
// GPIO 1 has internal pull-up enabled, button connects to GND
// Note: Using TX pin disables Serial output
// Debounce is handled inside ISR to prevent multiple triggers from switch bounce
volatile bool buttonPressed = false;
volatile unsigned long lastISRTime = 0;
volatile bool lastButtonState = true;  // true = HIGH (not pressed), false = LOW (pressed)

void IRAM_ATTR buttonISR() {
    unsigned long now = millis();
    bool currentState = digitalRead(BUTTON_PIN);
    
    // Only trigger on actual press (HIGH -> LOW transition)
    // and only if enough time has passed since last valid press
    if (currentState == LOW && lastButtonState == true && (now - lastISRTime > 500)) {
        buttonPressed = true;
        lastISRTime = now;
    }
    lastButtonState = currentState;
}

// =============================================================================
// Backlight Control
// =============================================================================
void setBacklight(bool on) {
    digitalWrite(LCD_BACKLIGHT, on ? HIGH : LOW);
}

// =============================================================================
// I2C Scanner - Diagnose connected devices
// =============================================================================
void scanI2C(int sda, int scl) {
    Wire.begin(sda, scl);
    delay(100);
    
    // Note: Serial output disabled (TX pin used for button)
    // Scan still runs to initialize I2C bus
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        Wire.endTransmission();
    }
}

// =============================================================================
// Read All Sensors
// =============================================================================
void readSensors() {
    // Read Chamber sensors (I2C Bus 1)
    Wire.begin(I2C1_SDA, I2C1_SCL);
    yield();
    
    data.chamberValid = chamberENS.isConnected();
    if (data.chamberValid) {
        data.chamberTVOC = chamberENS.getTVOC();
        data.chamberECO2 = chamberENS.getECO2();
        
        // Check for invalid readings - sensor may need re-init after hot-plug
        if (data.chamberTVOC == 0 && data.chamberECO2 == 0) {
            // Try to re-initialize
            if (chamberENS.begin(Wire, ENS160_ADDR) || chamberENS.begin(Wire, ENS160_ADDR_ALT)) {
                chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
                yield();
            }
        }
        
        sensors_event_t humidity, temp;
        if (chamberAHT.getEvent(&humidity, &temp)) {
            data.chamberTemp = (int)temp.temperature;
            data.chamberHumidity = (int)humidity.relative_humidity;
            chamberENS.setTempCompensationCelsius(temp.temperature);
            chamberENS.setRHCompensationFloat(humidity.relative_humidity);
        }
    } else {
        // Sensor not connected - try to recover
        if (chamberENS.begin(Wire, ENS160_ADDR) || chamberENS.begin(Wire, ENS160_ADDR_ALT)) {
            chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
            data.chamberValid = true;
            yield();
        }
    }
    
    // Read Room sensors (I2C Bus 2)
    Wire.begin(I2C2_SDA, I2C2_SCL);
    yield();
    
    data.roomValid = roomENS.isConnected();
    if (data.roomValid) {
        data.roomTVOC = roomENS.getTVOC();
        data.roomECO2 = roomENS.getECO2();
        
        // Check for invalid readings - sensor may need re-init after hot-plug
        if (data.roomTVOC == 0 && data.roomECO2 == 0) {
            // Try to re-initialize
            if (roomENS.begin(Wire, ENS160_ADDR) || roomENS.begin(Wire, ENS160_ADDR_ALT)) {
                roomENS.setOperatingMode(SFE_ENS160_STANDARD);
                yield();
            }
        }
        
        sensors_event_t humidity, temp;
        if (roomAHT.getEvent(&humidity, &temp)) {
            data.roomTemp = (int)temp.temperature;
            data.roomHumidity = (int)humidity.relative_humidity;
            roomENS.setTempCompensationCelsius(temp.temperature);
            roomENS.setRHCompensationFloat(humidity.relative_humidity);
        }
    } else {
        // Sensor not connected - try to recover
        if (roomENS.begin(Wire, ENS160_ADDR) || roomENS.begin(Wire, ENS160_ADDR_ALT)) {
            roomENS.setOperatingMode(SFE_ENS160_STANDARD);
            data.roomValid = true;
            yield();
        }
    }
}

// =============================================================================
// Update Running Average (Welford's algorithm)
// =============================================================================
void updateRunningAverage() {
    // Always sample - when sensors are invalid, data.* retains last valid values
    // This keeps the time axis accurate (1 history point = 1 minute of wall clock)
    sampleCount++;
    
    // Incremental average: avg = avg + (new - avg) / count
    avgChamberTVOC += (data.chamberTVOC - avgChamberTVOC) / sampleCount;
    avgChamberECO2 += (data.chamberECO2 - avgChamberECO2) / sampleCount;
    avgChamberTemp += (data.chamberTemp - avgChamberTemp) / sampleCount;
    avgChamberRH   += (data.chamberHumidity - avgChamberRH) / sampleCount;
    avgRoomTVOC    += (data.roomTVOC - avgRoomTVOC) / sampleCount;
    avgRoomECO2    += (data.roomECO2 - avgRoomECO2) / sampleCount;
    avgRoomTemp    += (data.roomTemp - avgRoomTemp) / sampleCount;
    avgRoomRH      += (data.roomHumidity - avgRoomRH) / sampleCount;
    
    // After 60 samples (1 minute), store to history buffer
    if (sampleCount >= SAMPLES_PER_MINUTE) {
        uint8_t writeIndex = historyPointCount % HISTORY_SIZE;
        history[writeIndex].chamberTVOC = (uint16_t)avgChamberTVOC;
        history[writeIndex].chamberECO2 = (uint16_t)avgChamberECO2;
        history[writeIndex].chamberTemp = (int16_t)avgChamberTemp;
        history[writeIndex].chamberRH   = (uint16_t)avgChamberRH;
        history[writeIndex].roomTVOC    = (uint16_t)avgRoomTVOC;
        history[writeIndex].roomECO2    = (uint16_t)avgRoomECO2;
        history[writeIndex].roomTemp    = (int16_t)avgRoomTemp;
        history[writeIndex].roomRH      = (uint16_t)avgRoomRH;
        
        historyPointCount++;
        
        // Reset for next minute
        avgChamberTVOC = avgChamberECO2 = avgChamberTemp = avgChamberRH = 0;
        avgRoomTVOC = avgRoomECO2 = avgRoomTemp = avgRoomRH = 0;
        sampleCount = 0;
    }
}

// =============================================================================
// Uptime Helper
// =============================================================================
void formatUptime(char* buf, size_t bufSize) {
    unsigned long uptimeSec = millis() / 1000;
    unsigned long days = uptimeSec / 86400;
    unsigned long hours = (uptimeSec % 86400) / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    
    if (days > 0) {
        snprintf(buf, bufSize, "%lud %luh %lum", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(buf, bufSize, "%luh %lum", hours, minutes);
    } else {
        snprintf(buf, bufSize, "%lum", minutes);
    }
}

// =============================================================================
// Display Functions
// =============================================================================
void drawMainScreen() {
    char buf[24];
    int textWidth;
    u8g2.setFont(u8g2_font_5x8_tf);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawVLine(64, 0, 44);
    u8g2.drawHLine(0, 10, 128);
    u8g2.drawHLine(0, 44, 128);
    
    u8g2.drawStr(14, 8, "CHAMBER");
    u8g2.drawStr(82, 8, "ROOM");
    
    // Chamber column
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
        // Chamber sensor error - flash ERROR text in sync with alarm beep
        u8g2.drawStr(2, 20, "TVOC:");
        u8g2.drawStr(2, 30, "eCO2:");
        
        if (errorBlinkOn) {
            // Inverted: white text on black background (error blink ON phase)
            u8g2.setDrawColor(1);
            u8g2.drawBox(36, 12, 26, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(37, 20, "ERROR");
            u8g2.setDrawColor(1);
            
            u8g2.drawBox(36, 22, 26, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(37, 30, "ERROR");
            u8g2.setDrawColor(1);
            
            u8g2.drawBox(1, 32, 62, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(2, 40, "ERROR");
            u8g2.setDrawColor(1);
        } else {
            // Normal: black text on white background (error blink OFF phase)
            u8g2.drawStr(37, 20, "ERROR");
            u8g2.drawStr(37, 30, "ERROR");
            u8g2.drawStr(2, 40, "ERROR");
        }
    }
    
    // Room column
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
        // Room sensor error - flash ERROR text in sync with alarm beep
        u8g2.drawStr(66, 20, "TVOC:");
        u8g2.drawStr(66, 30, "eCO2:");
        
        if (errorBlinkOn) {
            // Inverted: white text on black background (error blink ON phase)
            u8g2.setDrawColor(1);
            u8g2.drawBox(100, 12, 26, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(101, 20, "ERROR");
            u8g2.setDrawColor(1);
            
            u8g2.drawBox(100, 22, 26, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(101, 30, "ERROR");
            u8g2.setDrawColor(1);
            
            u8g2.drawBox(65, 32, 62, 10);
            u8g2.setDrawColor(0);
            u8g2.drawStr(66, 40, "ERROR");
            u8g2.setDrawColor(1);
        } else {
            // Normal: black text on white background (error blink OFF phase)
            u8g2.drawStr(101, 20, "ERROR");
            u8g2.drawStr(101, 30, "ERROR");
            u8g2.drawStr(66, 40, "ERROR");
        }
    }
    
    // WiFi status line
    #if WIFI_ENABLED
    if (wifiConnected) {
        snprintf(buf, sizeof(buf), "WiFi: %s", WiFi.localIP().toString().c_str());
        u8g2.drawStr(2, 52, buf);
    } else {
        u8g2.drawStr(2, 52, "WiFi: Disconnected");
    }
    #else
    u8g2.drawStr(2, 52, "WiFi: Disabled");
    #endif
    
    // Sensor warmup status line (countdown: 0 = warmed up)
    if (!data.chamberValid || !data.roomValid) {
        u8g2.drawStr(2, 62, "Sensors: Error");
    } else if (sensorWarmupCountdown > 0) {
        snprintf(buf, sizeof(buf), "Sensors: Warming %dm", (sensorWarmupCountdown + 59) / 60);
        u8g2.drawStr(2, 62, buf);
    } else {
        u8g2.drawStr(2, 62, "Sensors: Ready");
    }
}

// =============================================================================
// Boot Screen (shown during initialization)
// =============================================================================
void drawBootScreen(const char* status, uint8_t progress) {
    u8g2.clearBuffer();
    
    // Title - use slightly larger font for "VOC Monitor"
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(28, 20, "VOC Monitor");
    
    // Status message
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(4, 38, status);
    
    // Progress dots (up to 10 dots)
    if (progress > 0) {
        char dots[12];
        uint8_t numDots = progress > 10 ? 10 : progress;
        for (uint8_t i = 0; i < numDots; i++) dots[i] = '.';
        dots[numDots] = '\0';
        u8g2.drawStr(4, 50, dots);
    }
    
    u8g2.sendBuffer();
}

void drawStatusScreen() {
    char buf[26];
    u8g2.setFont(u8g2_font_5x8_tf);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawHLine(0, 10, 128);
    u8g2.drawStr(45, 8, "STATUS");
    
    // Uptime display
    char uptimeStr[16];
    formatUptime(uptimeStr, sizeof(uptimeStr));
    snprintf(buf, sizeof(buf), "Uptime: %s", uptimeStr);
    u8g2.drawStr(2, 20, buf);
    
    #if WIFI_ENABLED
    u8g2.drawStr(2, 30, "Web Dashboard: ON");
    #else
    u8g2.drawStr(2, 30, "Web Dashboard: OFF");
    #endif
    
    unsigned long validPoints = min(historyPointCount, (unsigned long)HISTORY_SIZE);
    snprintf(buf, sizeof(buf), "History: %lu/%d pts", validPoints, HISTORY_SIZE);
    u8g2.drawStr(2, 40, buf);
    
    snprintf(buf, sizeof(buf), "Avg samples: %d/60", sampleCount);
    u8g2.drawStr(2, 50, buf);
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
    // Note: Serial is disabled because TX pin (GPIO 1) is used for button
    // Serial.begin(115200);
    // delay(500);
    // Serial.println("\n=== VOC Monitor (ESP8266) ===\n");
    
    // Button on TX (GPIO 1) - interrupt-based detection with state tracking
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);
    
    // Backlight
    pinMode(LCD_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_BACKLIGHT, HIGH);
    
    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // LCD
    u8g2.begin();
    u8g2.setPowerSave(0);
    
    // Buzzer test beep
    tone(BUZZER_PIN, 1000, 200);  // 1kHz for 200ms
    delay(300);
    noTone(BUZZER_PIN);
    
    // Show boot screen
    drawBootScreen("Initializing...", 0);
    
    // Scan I2C buses to help diagnose connection issues
    drawBootScreen("Scanning I2C bus 1...", 1);
    scanI2C(I2C1_SDA, I2C1_SCL);
    
    drawBootScreen("Scanning I2C bus 2...", 2);
    scanI2C(I2C2_SDA, I2C2_SCL);
    
    // Chamber sensors (I2C1)
    drawBootScreen("Init chamber sensors...", 3);
    Wire.begin(I2C1_SDA, I2C1_SCL);
    delay(100);
    
    // Try default address first, then alternate address for ENS160
    if (chamberENS.begin(Wire, ENS160_ADDR)) {
        chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.chamberValid = true;
    } else if (chamberENS.begin(Wire, ENS160_ADDR_ALT)) {
        chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.chamberValid = true;
    }
    
    chamberAHT.begin(&Wire, 0, AHT20_ADDR);
    
    // Room sensors (I2C2)
    drawBootScreen("Init room sensors...", 4);
    Wire.begin(I2C2_SDA, I2C2_SCL);
    delay(100);
    
    // Try default address first, then alternate address for ENS160
    if (roomENS.begin(Wire, ENS160_ADDR)) {
        roomENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.roomValid = true;
    } else if (roomENS.begin(Wire, ENS160_ADDR_ALT)) {
        roomENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.roomValid = true;
    }
    
    roomAHT.begin(&Wire, 0, AHT20_ADDR);
    
    // WiFi
    #if WIFI_ENABLED
    connectWiFi();
    #endif
    
    // Initial read
    drawBootScreen("Starting...", 0);
    delay(500);
    readSensors();
    updateDisplay();
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    unsigned long now = millis();
    bool updateScreen = false;

    // 1. BUTTON - Debounce handled in ISR, just process the flag here
    if (buttonPressed) {
        buttonPressed = false;
        currentScreen = (ScreenState)((currentScreen + 1) % 3);
        updateScreen = true;
    }

    // 2. SENSORS & DISPLAY
    static unsigned long lastRead = 0;
    
    if (now - lastRead >= SENSOR_READ_INTERVAL) {
        lastRead = now;
        readSensors();
        updateRunningAverage();

        if (data.chamberValid && data.roomValid) {
            errorBlinkOn = false;
            if (sensorWarmupCountdown > 0) sensorWarmupCountdown -= (SENSOR_READ_INTERVAL / 1000);
        } else {
            errorBlinkOn = !errorBlinkOn;
            sensorWarmupCountdown = WARMUP_MINUTES * 60;
        }
        updateScreen = true;
    }

    // 3. CONTINUOUS TASKS
    if (updateScreen) updateDisplay();
    updateBuzzer();
    yield(); 
}