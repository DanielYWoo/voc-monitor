/**
 * VoC Monitor - Environmental Monitoring System
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
 * Button: D3 (GPIO 0) to GND
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
    .chamberTVOC = 0, .chamberECO2 = 400, .chamberTemp = 25, .chamberHumidity = 50, .chamberValid = false,
    .roomTVOC = 0, .roomECO2 = 400, .roomTemp = 25, .roomHumidity = 50, .roomValid = false
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
uint8_t historyHead = 0;             // Next write position
uint8_t historyCount = 0;            // Valid entries (0-30)

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
ScreenState currentScreen = SCREEN_MAIN;
volatile bool screenChanged = false;
volatile bool buttonPressed = false;

// =============================================================================
// WiFi and Web Server
// =============================================================================
#if WIFI_ENABLED
bool wifiConnected = false;
AsyncWebServer server(80);

// HTML page stored in PROGMEM
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VoC Monitor</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }
        h1 { text-align: center; margin-bottom: 20px; color: #00d4ff; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 20px; }
        .card { background: #16213e; border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .card h2 { font-size: 1.1em; color: #888; margin-bottom: 15px; text-transform: uppercase; letter-spacing: 1px; }
        .value { font-size: 2.5em; font-weight: bold; }
        .unit { font-size: 0.5em; color: #888; }
        .chamber { color: #ff6b6b; }
        .room { color: #4ecdc4; }
        .chart-container { background: #16213e; border-radius: 12px; padding: 20px; margin-bottom: 20px; }
        .status { text-align: center; color: #666; font-size: 0.9em; margin-top: 20px; }
        .good { color: #4ecdc4; }
        .warning { color: #ffd93d; }
        .danger { color: #ff6b6b; }
    </style>
</head>
<body>
    <h1>🌬️ VoC Monitor</h1>
    
    <div class="grid">
        <div class="card">
            <h2>Chamber</h2>
            <div class="value chamber"><span id="ch-tvoc">--</span><span class="unit"> ppb TVOC</span></div>
            <div class="value chamber"><span id="ch-eco2">--</span><span class="unit"> ppm eCO₂</span></div>
            <div style="margin-top:10px;color:#888"><span id="ch-temp">--</span>°C | <span id="ch-rh">--</span>% RH</div>
        </div>
        <div class="card">
            <h2>Room</h2>
            <div class="value room"><span id="rm-tvoc">--</span><span class="unit"> ppb TVOC</span></div>
            <div class="value room"><span id="rm-eco2">--</span><span class="unit"> ppm eCO₂</span></div>
            <div style="margin-top:10px;color:#888"><span id="rm-temp">--</span>°C | <span id="rm-rh">--</span>% RH</div>
        </div>
    </div>
    
    <div class="chart-container">
        <canvas id="tvocChart" height="120"></canvas>
    </div>
    <div class="chart-container">
        <canvas id="eco2Chart" height="120"></canvas>
    </div>
    
    <div class="status">Last update: <span id="lastUpdate">--</span></div>
    
    <script>
        const tvocCtx = document.getElementById('tvocChart').getContext('2d');
        const eco2Ctx = document.getElementById('eco2Chart').getContext('2d');
        
        const chartOptions = {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { grid: { color: '#333' }, ticks: { color: '#888' } },
                y: { grid: { color: '#333' }, ticks: { color: '#888' }, beginAtZero: true }
            },
            plugins: { legend: { labels: { color: '#888' } } }
        };
        
        const tvocChart = new Chart(tvocCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    { label: 'Chamber TVOC (ppb)', data: [], borderColor: '#ff6b6b', tension: 0.3, fill: false },
                    { label: 'Room TVOC (ppb)', data: [], borderColor: '#4ecdc4', tension: 0.3, fill: false }
                ]
            },
            options: { ...chartOptions, plugins: { ...chartOptions.plugins, title: { display: true, text: 'TVOC History (30 min)', color: '#888' } } }
        });
        
        const eco2Chart = new Chart(eco2Ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    { label: 'Chamber eCO2 (ppm)', data: [], borderColor: '#ff6b6b', tension: 0.3, fill: false },
                    { label: 'Room eCO2 (ppm)', data: [], borderColor: '#4ecdc4', tension: 0.3, fill: false }
                ]
            },
            options: { ...chartOptions, plugins: { ...chartOptions.plugins, title: { display: true, text: 'eCO2 History (30 min)', color: '#888' } } }
        });
        
        async function fetchCurrent() {
            try {
                const res = await fetch('/api/current');
                const d = await res.json();
                document.getElementById('ch-tvoc').textContent = d.ch_tvoc;
                document.getElementById('ch-eco2').textContent = d.ch_eco2;
                document.getElementById('ch-temp').textContent = d.ch_temp;
                document.getElementById('ch-rh').textContent = d.ch_rh;
                document.getElementById('rm-tvoc').textContent = d.rm_tvoc;
                document.getElementById('rm-eco2').textContent = d.rm_eco2;
                document.getElementById('rm-temp').textContent = d.rm_temp;
                document.getElementById('rm-rh').textContent = d.rm_rh;
                document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
            } catch (e) { console.error('Fetch error:', e); }
        }
        
        async function fetchHistory() {
            try {
                const res = await fetch('/api/data');
                const data = await res.json();
                const labels = data.map((_, i) => `-${data.length - i}m`);
                tvocChart.data.labels = labels;
                tvocChart.data.datasets[0].data = data.map(d => d.ch_tvoc);
                tvocChart.data.datasets[1].data = data.map(d => d.rm_tvoc);
                tvocChart.update();
                eco2Chart.data.labels = labels;
                eco2Chart.data.datasets[0].data = data.map(d => d.ch_eco2);
                eco2Chart.data.datasets[1].data = data.map(d => d.rm_eco2);
                eco2Chart.update();
            } catch (e) { console.error('Fetch error:', e); }
        }
        
        fetchCurrent();
        fetchHistory();
        setInterval(fetchCurrent, 5000);
        setInterval(fetchHistory, 30000);
    </script>
</body>
</html>
)rawliteral";

void setupWebServer() {
    // Serve main HTML page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    
    // API: Current readings (including running average)
    server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        // Use running average if we have samples, otherwise use last reading
        if (sampleCount > 0) {
            doc["ch_tvoc"] = (int)avgChamberTVOC;
            doc["ch_eco2"] = (int)avgChamberECO2;
            doc["ch_temp"] = (int)avgChamberTemp;
            doc["ch_rh"] = (int)avgChamberRH;
            doc["rm_tvoc"] = (int)avgRoomTVOC;
            doc["rm_eco2"] = (int)avgRoomECO2;
            doc["rm_temp"] = (int)avgRoomTemp;
            doc["rm_rh"] = (int)avgRoomRH;
        } else {
            doc["ch_tvoc"] = data.chamberTVOC;
            doc["ch_eco2"] = data.chamberECO2;
            doc["ch_temp"] = data.chamberTemp;
            doc["ch_rh"] = data.chamberHumidity;
            doc["rm_tvoc"] = data.roomTVOC;
            doc["rm_eco2"] = data.roomECO2;
            doc["rm_temp"] = data.roomTemp;
            doc["rm_rh"] = data.roomHumidity;
        }
        doc["samples"] = sampleCount;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    
    // API: Historical data (30 points)
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<2048> doc;
        JsonArray arr = doc.to<JsonArray>();
        
        // Output oldest to newest
        for (int i = 0; i < historyCount; i++) {
            int idx = (historyHead - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
            JsonObject point = arr.createNestedObject();
            point["ch_tvoc"] = history[idx].chamberTVOC;
            point["ch_eco2"] = history[idx].chamberECO2;
            point["ch_temp"] = history[idx].chamberTemp;
            point["ch_rh"] = history[idx].chamberRH;
            point["rm_tvoc"] = history[idx].roomTVOC;
            point["rm_eco2"] = history[idx].roomECO2;
            point["rm_temp"] = history[idx].roomTemp;
            point["rm_rh"] = history[idx].roomRH;
        }
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    
    server.begin();
    Serial.println("Web server started");
}

void connectWiFi() {
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
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
        setupWebServer();
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed! Will retry automatically.");
    }
}
#endif

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
        buttonPressed = true;
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
    // Read Chamber sensors (I2C Bus 1)
    Wire.begin(I2C1_SDA, I2C1_SCL);
    delay(10);
    
    data.chamberValid = chamberENS.isConnected();
    if (data.chamberValid) {
        data.chamberTVOC = chamberENS.getTVOC();
        data.chamberECO2 = chamberENS.getECO2();
        
        sensors_event_t humidity, temp;
        if (chamberAHT.getEvent(&humidity, &temp)) {
            data.chamberTemp = (int)temp.temperature;
            data.chamberHumidity = (int)humidity.relative_humidity;
            chamberENS.setTempCompensationCelsius(temp.temperature);
            chamberENS.setRHCompensationFloat(humidity.relative_humidity);
        }
    }
    
    // Read Room sensors (I2C Bus 2)
    Wire.begin(I2C2_SDA, I2C2_SCL);
    delay(10);
    
    data.roomValid = roomENS.isConnected();
    if (data.roomValid) {
        data.roomTVOC = roomENS.getTVOC();
        data.roomECO2 = roomENS.getECO2();
        
        sensors_event_t humidity, temp;
        if (roomAHT.getEvent(&humidity, &temp)) {
            data.roomTemp = (int)temp.temperature;
            data.roomHumidity = (int)humidity.relative_humidity;
            roomENS.setTempCompensationCelsius(temp.temperature);
            roomENS.setRHCompensationFloat(humidity.relative_humidity);
        }
    }
}

// =============================================================================
// Update Running Average (Welford's algorithm)
// =============================================================================
void updateRunningAverage() {
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
        history[historyHead].chamberTVOC = (uint16_t)avgChamberTVOC;
        history[historyHead].chamberECO2 = (uint16_t)avgChamberECO2;
        history[historyHead].chamberTemp = (int16_t)avgChamberTemp;
        history[historyHead].chamberRH   = (uint16_t)avgChamberRH;
        history[historyHead].roomTVOC    = (uint16_t)avgRoomTVOC;
        history[historyHead].roomECO2    = (uint16_t)avgRoomECO2;
        history[historyHead].roomTemp    = (int16_t)avgRoomTemp;
        history[historyHead].roomRH      = (uint16_t)avgRoomRH;
        
        // Advance circular buffer
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        if (historyCount < HISTORY_SIZE) historyCount++;
        
        // Reset for next minute
        avgChamberTVOC = avgChamberECO2 = avgChamberTemp = avgChamberRH = 0;
        avgRoomTVOC = avgRoomECO2 = avgRoomTemp = avgRoomRH = 0;
        sampleCount = 0;
        
        Serial.printf("History: stored point %d/%d\n", historyCount, HISTORY_SIZE);
    }
}

// =============================================================================
// Display Functions
// =============================================================================
void drawMainScreen() {
    char buf[16];
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
        u8g2.drawStr(2, 20, "TVOC:");
        u8g2.drawStr(42, 20, "ERR");
        u8g2.drawStr(2, 30, "eCO2:");
        u8g2.drawStr(42, 30, "ERR");
        u8g2.drawStr(2, 40, "DISCONNECTED");
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
        u8g2.drawStr(66, 20, "TVOC:");
        u8g2.drawStr(106, 20, "ERR");
        u8g2.drawStr(66, 30, "eCO2:");
        u8g2.drawStr(106, 30, "ERR");
        u8g2.drawStr(66, 40, "DISCONNECTED");
    }
    
    // WiFi status
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
    u8g2.setFont(u8g2_font_5x8_tf);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawHLine(0, 10, 128);
    u8g2.drawStr(45, 8, "STATUS");
    
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
    
    u8g2.drawStr(2, 40, "Web Dashboard: ON");
    snprintf(buf, sizeof(buf), "History: %d/%d pts", historyCount, HISTORY_SIZE);
    u8g2.drawStr(2, 50, buf);
    snprintf(buf, sizeof(buf), "Avg samples: %d/60", sampleCount);
    u8g2.drawStr(2, 60, buf);
    #else
    u8g2.drawStr(2, 20, "WiFi: Disabled");
    u8g2.drawStr(2, 30, "IP: N/A");
    u8g2.drawStr(2, 40, "Web Dashboard: OFF");
    u8g2.drawStr(2, 50, "History: N/A");
    u8g2.drawStr(2, 60, "");
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
    
    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    // Backlight
    pinMode(LCD_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_BACKLIGHT, HIGH);
    
    // LCD
    Serial.println("Initializing LCD...");
    u8g2.begin();
    u8g2.setPowerSave(0);
    
    // Chamber sensors (I2C1)
    Serial.println("Initializing Chamber sensors...");
    Wire.begin(I2C1_SDA, I2C1_SCL);
    delay(100);
    
    if (chamberENS.begin(Wire, ENS160_ADDR)) {
        Serial.println("Chamber ENS160: OK");
        chamberENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.chamberValid = true;
    } else {
        Serial.println("Chamber ENS160: NOT FOUND");
    }
    
    if (chamberAHT.begin(&Wire, 0, AHT20_ADDR)) {
        Serial.println("Chamber AHT20: OK");
    } else {
        Serial.println("Chamber AHT20: NOT FOUND");
    }
    
    // Room sensors (I2C2)
    Serial.println("Initializing Room sensors...");
    Wire.begin(I2C2_SDA, I2C2_SCL);
    delay(100);
    
    if (roomENS.begin(Wire, ENS160_ADDR)) {
        Serial.println("Room ENS160: OK");
        roomENS.setOperatingMode(SFE_ENS160_STANDARD);
        data.roomValid = true;
    } else {
        Serial.println("Room ENS160: NOT FOUND");
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
    Serial.println("Web dashboard available at http://<device-ip>/");
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    static unsigned long lastRead = 0;
    
    #if WIFI_ENABLED
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    #endif
    
    // Read sensors every 1 second
    if (millis() - lastRead >= SENSOR_READ_INTERVAL) {
        lastRead = millis();
        readSensors();
        updateRunningAverage();
        updateDisplay();
    }
    
    if (screenChanged) {
        screenChanged = false;
        updateDisplay();
    }
    
    if (buttonPressed) {
        buttonPressed = false;
        Serial.printf("Button pressed! Screen: %d\n", currentScreen);
    }
    
    delay(50);  // Yield for WiFi stack and async web server
}