/**
 * ESP8266 + ILI9341 2.2" TFT Hello World
 * For Arduino IDE
 * 
 * Required Libraries (install via Library Manager):
 *   - Adafruit GFX Library
 *   - Adafruit ILI9341
 * 
 * Board Settings in Arduino IDE:
 *   - Board: "NodeMCU 1.0 (ESP-12E Module)" or "LOLIN(WEMOS) D1 R2 & mini"
 *   - Upload Speed: 921600
 *   - CPU Frequency: 80 MHz
 *   - Flash Size: 4MB (FS:2MB OTA:~1019KB)
 * 
 * ILI9341 TFT Wiring:
 *   VCC  -> 3V3
 *   GND  -> GND
 *   CS   -> D8 (GPIO15)
 *   RST  -> D4 (GPIO2)
 *   DC   -> D3 (GPIO0)
 *   MOSI -> D7 (GPIO13)
 *   SCK  -> D5 (GPIO14)
 *   LED  -> 3V3
 *   MISO -> NC (not connected)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Pin definitions for NodeMCU ESP8266
#define TFT_CS   15  // D8 = GPIO15
#define TFT_DC    0  // D3 = GPIO0
#define TFT_RST   2  // D4 = GPIO2

// Hardware SPI: MOSI=GPIO13(D7), SCK=GPIO14(D5)
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n================================");
    Serial.println("ESP8266 + ILI9341 TFT (Adafruit)");
    Serial.println("================================");
    
    Serial.println("\nPin Configuration:");
    Serial.println("  TFT_CS   = GPIO15 (D8)");
    Serial.println("  TFT_DC   = GPIO0 (D3)");
    Serial.println("  TFT_RST  = GPIO2 (D4)");
    Serial.println("  MOSI     = GPIO13 (D7) - Hardware SPI");
    Serial.println("  SCK      = GPIO14 (D5) - Hardware SPI");
    
    Serial.println("\nInitializing TFT...");
    
    // Manual reset
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH);
    delay(10);
    digitalWrite(TFT_RST, LOW);
    delay(10);
    digitalWrite(TFT_RST, HIGH);
    delay(150);
    Serial.println("  Manual reset done");
    
    // Initialize display
    tft.begin();
    Serial.println("  tft.begin() done");
    
    // Set rotation (1 = landscape, 320x240)
    tft.setRotation(1);
    Serial.println("  setRotation(1) done");
    
    // Test with colors
    Serial.println("\nTesting colors...");
    
    Serial.println("  RED");
    tft.fillScreen(ILI9341_RED);
    delay(500);
    
    Serial.println("  GREEN");
    tft.fillScreen(ILI9341_GREEN);
    delay(500);
    
    Serial.println("  BLUE");
    tft.fillScreen(ILI9341_BLUE);
    delay(500);
    
    Serial.println("  BLACK");
    tft.fillScreen(ILI9341_BLACK);
    
    // Draw text
    Serial.println("\nDrawing text...");
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(4);
    tft.setCursor(100, 80);
    tft.println("Hello!");
    
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(2);
    tft.setCursor(40, 150);
    tft.println("ESP8266 + ILI9341");
    
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(1);
    tft.setCursor(100, 200);
    tft.println("320x240 TFT Display");
    
    Serial.println("\n================================");
    Serial.println("Done! Check display.");
    Serial.println("================================");
}

void loop() {
    static int x = 0;
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 30) {
        lastUpdate = millis();
        tft.fillRect(x, 220, 20, 10, ILI9341_BLACK);
        x = (x + 5) % 300;
        tft.fillRect(x, 220, 20, 10, ILI9341_YELLOW);
    }
    yield();  // ESP8266 watchdog
}