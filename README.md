🌍 All Event Sources  
SourceEventsStatusUSGSEarthquakes M4.5+  
✅ WorkingGDACSCyclones, Floods, Volcanoes, Droughts, Wildfires   
✅ WorkingNASA EONETWildfires, Storms, Volcanoes, Icebergs   

✅ WorkingNOAA SpaceSolar Flares, Geomagnetic Storms, CME, Radio Blackouts  

✅ NEW
☀️ Space Weather Events
DisplayMeaningDangerFLARESolar X-ray flare
🔴 Can disrupt radioGEOMAGGeomagnetic storm (G1-G5)
🔴 Power grid issuesCMECoronal Mass Ejection
🔴 Satellites at riskRADIORadio blackout  
🟠 HF radio affectedSOLARGeneral solar activity  
🟢 Monitoring  

⚠️ About DEFCON / War Monitoring  
Unfortunately, there's no public API for DEFCON levels -   
it's classified military information. However,   
I added the event types so if you find a source later, you can use:  

WAR - Armed conflict  
DEFCON - Defense condition  
NUKE - Nuclear event  
MILITARY - Military activity  
CONFLICT - General conflict  
TERROR - Terror incident   

🔮 Other Cool Sources You Could Add Later
SourceURLEventsRSOE EDISedis.rsoe.huAll disasters worldwideLiveUAMapliveuamap.comConflicts 
(no free API)Flightradar24fr24.comPlane incidents (paid API)WHO Diseasewho.intDisease outbreaks

![photo_2026-02-25_16-25-18 (2)](https://github.com/user-attachments/assets/2536bfad-33c0-4cdc-87f8-5cb56b456f98)
![photo_2026-02-25_16-23-12](https://github.com/user-attachments/assets/6fa6cb43-5288-4170-adb3-a02a5424922b)
![photo_2026-02-25_16-25-18](https://github.com/user-attachments/assets/1972b9ba-1890-4358-a5da-91dca01708c0)
![Screenshot_2026-02-26-20-27-49-525_com geeksville mesh](https://github.com/user-attachments/assets/c7b079c4-c430-4675-a910-015e217bead1)


🛠️ Hardware Requirements
Component	Specification

Microcontroller	ESP32 TTGO T-Display
Display	1.14" ST7789V TFT (240x135)
LoRa Module	Meshtastic-compatible (Heltec, etc.)
Connectivity	WiFi 2.4GHz, UART @ 9600 baud
GPIO Used	Button 1 (35), Button 2 (0), Backlight (4), Mesh TX (27), Mesh RX (25)
3 wired


 # ESP32 TTGO T-Display Disaster Alert v2.2

## Overview
The **ESP32 TTGO T-Display Disaster Alert v2.2** is a disaster monitoring and mesh communication device. It fetches live disaster alerts (such as USGS earthquakes) via WiFi and displays them on a color TFT screen. To ensure alerts reach areas without internet, it interfaces with a secondary Meshtastic LoRa node (like a Heltec device) over serial to broadcast parsed alerts into a local LoRa mesh network. It can also receive incoming mesh chat messages and display them.


## Hardware Requirements
* **Primary Microcontroller:** ESP32 TTGO T-Display.
* **Secondary Node:** Meshtastic-compatible LoRa node (e.g., Heltec V3).
* **Display:** Built-in TFT Display.

### Pin Configuration & Wiring
The TTGO T-Display communicates with the Meshtastic node via UART:
* **TFT Backlight:** GPIO 4.
* **Button 1 (Right/USB side):** GPIO 35.
* **Button 2 (Left):** GPIO 0.
* **Mesh TX (TTGO):** GPIO 27 (connects to Heltec RX pin 48).
* **Mesh RX (TTGO):** GPIO 25 (connects to Heltec TX pin 47).
* **Serial Baud Rate:** 9600.

## Software Dependencies
This project requires the following libraries:
* `WiFi.h` & `WiFiClientSecure.h`
* `HTTPClient.h`
* `EEPROM.h`
* `ArduinoJson` (v7)
* `SPI.h`
* `TFT_eSPI`

## Core Features
* **Live USGS Tracking:** Routinely fetches earthquake data from the USGS GeoJSON feed.
* **LoRa Mesh Integration:** Formats disaster events and forwards them over serial to a Meshtastic node for off-grid broadcasting.
* **Mesh Chat Monitor:** Actively listens to the Meshtastic node's serial output and displays incoming chat messages directly on the TTGO screen.
* **Duplicate Alert Prevention:** Keeps track of "seen" events using the ESP32's EEPROM.
* **Memory Protection:** Utilizes incremental stream parsing for large JSON payloads to prevent out-of-memory crashes. Stack memory is actively monitored.
* **UTF-8 Sanitization:** Strips special characters and converts UTF-8 strings into ASCII to ensure safe transmission over the LoRa network.

## System Architecture

The project is structured into four primary modules:

### 1. `main.cpp` (Application Core)
* Initializes WiFi connection, hardware buttons, and the TFT display.
* Contains the primary event loop and handles timing controls like `FETCH_INTERVAL_MS` (5 minutes) and `DISPLAY_DURATION_MS` (8 seconds).
* Implements a LoRa message queue (`loraQueue`) to rate-limit messages sent to the Meshtastic device to prevent network flooding.
* Includes an emergency clear-and-reboot mechanism triggered by holding a button for 3 seconds.

### 2. `json_parser.cpp` (Network & Parsing)
* Manages HTTP GET requests to disaster data APIs via the `fetch_usgs_data` function.
* Employs ArduinoJson v7 to stream directly from the `WiFiClient` pointer, bypassing large string allocations.
* Incorporates a `sanitize_for_lora` function to strip out invalid characters and handle UTF-8 diacritics before queuing the text.

### 3. `event_tracker.cpp` (Storage & State)
* Defines a secure EEPROM structure (`eeprom_data_t`) including magic bytes (`0xDA`), versioning, and validation checks.
* Contains safe read/write wrapper functions (`safe_eeprom_read` / `safe_eeprom_write`) that prevent out-of-bounds memory access.
* Maintains a display queue (`disaster_event_t`) and a cyclic history of previously seen disaster IDs.
* Uses a delayed save mechanism (`EEPROM_SAVE_DELAY`) to minimize wear and tear on the flash memory.

### 4. `display_renderer.cpp` (User Interface)
* Interfaces with the `TFT_eSPI` library to draw elements on the screen.
* Contains dedicated screen states, including `display_startup_screen`, `display_status`, `display_error`, and `display_wifi_connecting`.
* Features dynamic rendering functions like `display_next_alert` (which extracts alert colors depending on the severity level).
![HCBqlCAWMAE_qxc](https://github.com/user-attachments/assets/3033b22b-7db0-4ed9-8f70-1275e6095c25)


https://github.com/user-attachments/assets/caeeeeb9-33f7-46c9-97f5-eaea63c303ac


