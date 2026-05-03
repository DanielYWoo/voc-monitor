
This specification outlines the design and implementation of an industrial-grade environmental monitoring and filtration system for a 3D printer enclosure (Voron Trident and Bedslinger), specifically optimized for safe ABS printing.

This is a standalone project, not tightly coupled to klipper or the Raspberry Pi ecosystem. It can be used for non-3d-printing scenarios too.

1. Monitoring Suite (The "Dual-Layer" Sensor Array)
The system uses a redundant array to distinguish between real VOC spikes and environmental drift.
Primary Precision Layer (I2C):
2x ENS160 + AHT20 Modules: (One for Chamber, one for Room).
Capabilities: TVOC (ppb), eCO2, and high-accuracy Temperature/Humidity.
Algorithm: The AHT20 provides live temp/humidity data to the ENS160’s internal ASIC to compensate for environmental drift in real-time.
Legacy/Backup Layer (Analog/1-Wire):
MQ135 Sensor: Dedicated to detecting Benzene/Styrene. Connected via a 3.3V voltage divider to the ESP32.
DS18B20 Digital Thermometer: Used as a hardware "sanity check" to detect failure/drift in the AHT20 sensors.
Software Correction: Use the MQUnifiedsensor library to apply logarithmic temperature/humidity compensation to the MQ135 analog signal.
2. Controller & Interface (The Brain)
MCU: ESP32 (Dual-Core, 240MHz).
Requirement Logic: Dual hardware I2C buses are required to run two AHT20 sensors (fixed address 0x38). High RAM/Flash is required for the TFT GUI and WiFi stack.
Display: 2.4" TFT SPI QVGA (240x320) using TFT_eSPI library.
Physical Controls:
One physical button wired to GPIO (Input Pullup).
Functions: Toggle LCD backlight (LED/BLK pin) via digitalWrite for "Dark Mode"; long-press for emergency functions.
Power: 5V/1A dedicated supply (Pi-powered or standby 5V) to ensure 24/7 operation independent of the printer’s main 24V PSU.
Standby: ESP32 is always ON. VOC levels in the room are monitored 24/7.
If Room VOCs (Sensor #2) spike, the ESP32 triggers a visual alarm on the TFT.
3. Integration & Connectivity
This is optional and can be turned off by a flag.
WiFi Integration: ESP32 uses a non-blocking WiFi client to send data to the Raspberry Pi.
Mainsail/Fluidd Dashboard:
Data is pushed to the Moonraker API via HTTP POST or MQTT.
Custom sensor cards added to the dashboard to show real-time VOC levels (ppb) and Chamber Health.
Automation (Moonraker [power] plugin):
Component	ESP32 Pin	Notes
TFT SCK / MOSI	GPIO 18 / 23	Standard VSPI
TFT CS / DC / RST	GPIO 5 / 2 / 4	Display Control
TFT LED/BLK	GPIO 15	Backlight Toggle (Button Control)
I2C Bus 1 (Chamber)	GPIO 21 (SDA) / 22 (SCL)	ENS160 #1 + AHT20 #1
I2C Bus 2 (Room)	GPIO 16 (SDA) / 17 (SCL)	ENS160 #2 + AHT20 #2
MQ135 Analog	GPIO 32 / 33	Requires Voltage Divider (5V to 3.3V)
DS18B20 Data	GPIO 25	Requires 4.7k Pull-up
User Button	GPIO 26	To GND (Input Pullup)

4. Calibration

Sensor	Calibration Needed?
DHT11	❌ No	N/A	Factory calibrated, no user calibration possible
MQ135	✅ Yes	Once (or yearly)	Needs baseline Ro in clean air
ENS160	⚠️ Auto	Every power-on	Self-calibrates over 1 hour, no user action needed
AHT20	❌ No	N/A	Factory calibrated, no user calibration possible

Since MQ135 needs to be calibrated every year. We use a compile time flag to put it under calibration mode for 24 hours and save the data into the ROM.

This spec is used to generate the design.md