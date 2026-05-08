This specification outlines the design and implementation of an industrial-grade environmental monitoring and filtration system for a 3D printer enclosure (Voron Trident and Bedslinger), specifically optimized for safe ABS printing.

This is a standalone project, not tightly coupled to klipper or the Raspberry Pi ecosystem. It can be used for non-3d-printing scenarios too.

1. Monitoring Suite (The "Dual-Layer" Sensor Array)
The system uses a redundant array to distinguish between real VOC spikes and environmental drift.
Primary Precision Layer (I2C):
2x ENS160 + AHT20 Modules: (One for Chamber, one for Room).
Capabilities: TVOC (ppb), eCO2, and high-accuracy Temperature/Humidity.
Algorithm: The AHT20 provides live temp/humidity data to the ENS160’s internal ASIC to compensate for environmental drift in real-time.
Legacy/Backup Layer (Analog/1-Wire):
If the room sensor readings are high, then make the noise like a Geiger counter.
2. Controller & Interface (The Brain)
MCU: ESP8266 (Single-Core).
Requirement Logic: Dual hardware I2C buses are required to run two AHT20 sensors (fixed address 0x38). High RAM/Flash is required for the TFT GUI and WiFi stack.
Display: 12864 LCD 
Physical Controls:
One physical button wired to GPIO (Input Pullup).
Functions: Toggle LCD backlight (LED/BLK pin) via digitalWrite for "Dark Mode"; long-press for emergency functions.
Power: 5V/1A dedicated supply (Pi-powered or standby 5V) to ensure 24/7 operation independent of the printer’s main 24V PSU.
Standby: ESP8266 is always ON. VOC levels in the room are monitored 24/7.
3. Integration & Connectivity
This is optional and can be turned off by a flag.
WiFi Integration: ESP8266 uses a non-blocking web server to provide data.
Custom sensor cards added to the dashboard to show real-time VOC levels (ppb) and Chamber Health.
Automation (Moonraker [power] plugin):

4. Calibration
Sensor	Calibration Needed?
ENS160	⚠️ Auto	Every power-on	Self-calibrates over 1 hour, no user action needed
AHT20	❌ No	N/A	Factory calibrated, no user calibration possible

This spec is used to generate the design.md