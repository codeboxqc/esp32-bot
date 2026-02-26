#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// ==================== WiFi Configuration ====================
#define WIFI_SSID       "demonoid"
#define WIFI_PASSWORD   "lacasadepapel2019"

// ==================== Timing Configuration ====================
#define FETCH_INTERVAL_MS       (15UL * 60UL * 1000UL) 
#define DISPLAY_DURATION_MS     (10UL * 1000UL)        
#define WIFI_TIMEOUT_MS         30000                  
#define HTTP_TIMEOUT_MS         15000   



// ==================== Display (TFT_eSPI) ====================
#define TFT_BL_PIN      4      // TTGO T-Display Backlight

// ==================== Meshtastic (UART) ====================
#define MESH_TX_PIN     27
#define MESH_RX_PIN     25
#define MESH_BAUD       9600

// ==================== API Endpoints ====================
//#define USGS_URL        "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson"
//#define GDACS_URL       "https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH"

// ==================== API Endpoints ====================
#define USGS_URL        "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson"
#define GDACS_URL       "https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH?alertlevel=red"


// ==================== Event Tracking ====================
#define MAX_TRACKED_EVENTS  50
#define MAX_EVENT_ID_LEN    64
#define MAX_DISPLAY_QUEUE   10
#define MAX_TITLE_LEN       128
#define MAX_COUNTRY_LEN     64
#define MAX_TYPE_LEN        16

// ==================== EEPROM Storage ====================
#define EEPROM_SIZE         2048
#define EEPROM_MAGIC        0xDA
#define EEPROM_VERSION      1

// ==================== Global LoRa Function ====================
// Implemented in main.cpp, used by json_parser.cpp
void queue_lora_message(const char* message);

#endif  