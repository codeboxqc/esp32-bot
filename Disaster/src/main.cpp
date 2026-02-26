/*
 * ESP32 TTGO T-Display Disaster Alert v2.3 - PROTECTED VERSION
 * Added: EEPROM wear protection, memory safety, watchdog, brown-out protection
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_task_wdt.h>    // Watchdog
#include "soc/rtc_cntl_reg.h" // Brown-out detector

extern void display_mesh_chat(const char* message);

// ==================== WIFI ====================
const char* WIFI_SSID     = "demonoid";
const char* WIFI_PASSWORD = "lacasadepapel2019";

// ==================== TIMING ====================
#define FETCH_INTERVAL_MS   (5UL * 60UL * 1000UL)
#define DISPLAY_DURATION_MS (8UL * 1000UL)
#define WIFI_TIMEOUT_MS     30000

// ==================== TTGO HARDWARE ====================
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL_PIN 4
#define BUTTON_1   35
#define BUTTON_2   0

// ==================== MESHTASTIC ====================
#define MESH_TX_PIN 27
#define MESH_RX_PIN 25
#define MESH_BAUD   9600

// ==================== UART PROTECTION ====================
#define UART_NOISE_THRESHOLD    5       // Max garbage chars before reset
#define UART_MSG_MIN_LEN        3       // Minimum valid message length
#define UART_MSG_MAX_LEN        200     // Maximum valid message length
#define UART_TIMEOUT_MS         100     // Timeout for incomplete messages
#define UART_GARBAGE_RESET_MS   5000    // Reset garbage counter every 5s

static int uart_garbage_count = 0;
static unsigned long last_garbage_reset = 0;
static bool uart_healthy = true;

// ==================== API ENDPOINTS ====================
const char* USGS_URL = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson";
const char* GDACS_URL = "https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH?eventlist=EQ,TC,FL,VO,DR,WF&alertlevel=Green;Orange;Red&limit=10";
const char* EONET_URL = "https://eonet.gsfc.nasa.gov/api/v3/events?status=open&limit=10";
const char* NOAA_SPACE_URL = "https://services.swpc.noaa.gov/products/alerts.json";  // Solar flares, geomagnetic storms
const char* NUCLEAR_URL = "https://api.nuclearsecrecy.com/status.json";  // Unofficial - may not work

// ==================== EEPROM PROTECTION ====================
#define EEPROM_SIZE    512
#define EEPROM_MAGIC   0xDA
#define EEPROM_VERSION 0x02  // Bumped version for new format
#define MAX_EVENTS     20
#define ID_LENGTH      24

// *** EEPROM WEAR PROTECTION ***
#define EEPROM_MIN_SAVE_INTERVAL  30000   // Minimum 30 seconds between saves
#define EEPROM_MAX_SAVES_PER_HOUR 20      // Max 20 saves per hour
#define EEPROM_WRITE_COUNT_ADDR   500     // Store write count at end of EEPROM
#define EEPROM_MAX_LIFETIME_WRITES 100000 // ESP32 EEPROM rated for ~100k writes

static unsigned long last_eeprom_save_time = 0;
static uint16_t eeprom_saves_this_hour = 0;
static unsigned long hour_start_time = 0;
static uint32_t total_eeprom_writes = 0;
static bool eeprom_write_allowed = true;

// ==================== MEMORY PROTECTION ====================
#define MIN_FREE_HEAP       10000   // Minimum 10KB free heap
#define CRITICAL_FREE_HEAP  5000    // Critical: 5KB - stop operations
#define MAX_JSON_SIZE       20000   // 20KB - USGS payloads are ~13KB

// ==================== WATCHDOG ====================
#define WDT_TIMEOUT_SECONDS 30  // Reset if hung for 30 seconds

// ==================== OTHER SETTINGS ====================
#define BUTTON_HOLD_TIME    3000
#define STACK_MONITOR_INTERVAL 10000

char seenEvents[MAX_EVENTS][ID_LENGTH];
int  seenCount = 0;
int  seenIndex = 0;

// ==================== QUEUE ====================
struct DisasterEvent {
    char    id[ID_LENGTH];
    char    type[12];       // EQ, TC, FL, VO, WF, DR, storm, fire, etc.
    char    location[64];
    float   magnitude;
    uint8_t alertLevel;     // 0=green, 1=orange, 2=red
};

// Event type display names
const char* getEventTypeName(const char* code) {
    if (!code || strlen(code) == 0) return "ALERT";
    
    // Earthquakes & Geological
    if (strcmp(code, "EQ") == 0) return "QUAKE";
    if (strcmp(code, "VO") == 0) return "VOLCANO";
    if (strcmp(code, "volcano") == 0) return "VOLCANO";
    if (strcmp(code, "landslide") == 0) return "SLIDE";
    
    // Weather
    if (strcmp(code, "TC") == 0) return "CYCLONE";
    if (strcmp(code, "FL") == 0) return "FLOOD";
    if (strcmp(code, "flood") == 0) return "FLOOD";
    if (strcmp(code, "DR") == 0) return "DROUGHT";
    if (strcmp(code, "storm") == 0) return "STORM";
    if (strcmp(code, "severeStorm") == 0) return "STORM";
    if (strcmp(code, "iceberg") == 0) return "ICEBERG";
    if (strcmp(code, "snow") == 0) return "SNOW";
    
    // Fire
    if (strcmp(code, "WF") == 0) return "WILDFIRE";
    if (strcmp(code, "fire") == 0) return "FIRE";
    if (strcmp(code, "wildfire") == 0) return "FIRE";
    
    // Space Weather
    if (strcmp(code, "SOLAR") == 0) return "SOLAR";
    if (strcmp(code, "GEOMAG") == 0) return "GEOMAG";
    if (strcmp(code, "RADIO") == 0) return "RADIO";
    if (strcmp(code, "FLARE") == 0) return "FLARE";
    if (strcmp(code, "CME") == 0) return "CME";  // Coronal Mass Ejection
    
    // Military / Conflict
    if (strcmp(code, "WAR") == 0) return "WAR";
    if (strcmp(code, "DEFCON") == 0) return "DEFCON";
    if (strcmp(code, "NUKE") == 0) return "NUKE";
    if (strcmp(code, "MILITARY") == 0) return "MILITARY";
    if (strcmp(code, "CONFLICT") == 0) return "CONFLICT";
    if (strcmp(code, "TERROR") == 0) return "TERROR";
    if (strcmp(code, "PROTEST") == 0) return "PROTEST";
    
    // Health
    if (strcmp(code, "EPIDEMIC") == 0) return "EPIDEMIC";
    if (strcmp(code, "PANDEMIC") == 0) return "PANDEMIC";
    if (strcmp(code, "OUTBREAK") == 0) return "OUTBREAK";
    
    // Other
    if (strcmp(code, "CYBER") == 0) return "CYBER";
    if (strcmp(code, "PLANE") == 0) return "PLANE";
    if (strcmp(code, "SHIP") == 0) return "SHIP";
    if (strcmp(code, "TRAIN") == 0) return "TRAIN";
    
    return "ALERT";
}

// Alert level colors  
uint16_t getAlertColor(uint8_t level) {
    switch (level) {
        case 2: return TFT_RED;
        case 1: return TFT_ORANGE;
        default: return TFT_GREEN;
    }
}

DisasterEvent displayQueue[5];
int queueHead  = 0;
int queueTail  = 0;
int queueCount = 0;

// ==================== LORA QUEUE ====================
#define LORA_QUEUE_SIZE 5
char loraQueue[LORA_QUEUE_SIZE][80];
int  loraQueueCount = 0;

// ==================== FORWARD DECLARATIONS ====================
void monitor_mesh_chat();
void check_memory();
void check_buttons();
void emergency_clear_and_reboot();
void eeprom_clear(void);
void eeprom_save(void);
void feed_watchdog(void);
bool is_memory_safe(void);
bool can_save_eeprom(void);
void load_eeprom_write_count(void);
void save_eeprom_write_count(void);
void reset_uart_health(void);
void check_uart_health(void);
void flush_uart_garbage(void);
int fetchUSGS(void);
int fetchGDACS(void);
int fetchEONET(void);
int fetchSpaceWeather(void);
int fetchAllDisasters(void);

// ==================== GLOBALS ====================
unsigned long lastFetchTime     = 0;
unsigned long lastDisplayChange = 0;
bool          wifiConnected     = false;
DisasterEvent currentEvent;
bool          showingAlert      = false;

static unsigned long button1_hold_start = 0;
static unsigned long button2_hold_start = 0;
static bool button1_held = false;
static bool button2_held = false;

// ==================== WATCHDOG FUNCTIONS ====================

void init_watchdog() {
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);  // Enable panic on timeout
    esp_task_wdt_add(NULL);  // Add current thread to watchdog
    Serial.println("[WDT] Watchdog initialized");
}

void feed_watchdog() {
    esp_task_wdt_reset();
}

// ==================== MEMORY PROTECTION ====================

bool is_memory_safe() {
    uint32_t free_heap = ESP.getFreeHeap();
    return free_heap >= MIN_FREE_HEAP;
}

bool is_memory_critical() {
    uint32_t free_heap = ESP.getFreeHeap();
    return free_heap < CRITICAL_FREE_HEAP;
}

void check_memory() {
    static unsigned long last_memory_check = 0;
    if (millis() - last_memory_check > STACK_MONITOR_INTERVAL) {
        last_memory_check = millis();
        
        uint32_t free_heap = ESP.getFreeHeap();
        uint32_t min_free_heap = ESP.getMinFreeHeap();
        uint32_t max_alloc = ESP.getMaxAllocHeap();
        
        Serial.printf("[MEM] Free:%u Min:%u MaxAlloc:%u\n", 
                      free_heap, min_free_heap, max_alloc);
        
        if (free_heap < CRITICAL_FREE_HEAP) {
            Serial.println("[MEM] ⚠️ CRITICAL - Forcing restart!");
            delay(100);
            ESP.restart();
        } else if (free_heap < MIN_FREE_HEAP) {
            Serial.println("[MEM] ⚠️ LOW - Clearing queues");
            queueCount = 0;
            queueHead = 0;
            queueTail = 0;
            loraQueueCount = 0;
        }
    }
}

// ==================== EEPROM PROTECTION ====================

void load_eeprom_write_count() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read lifetime write count from end of EEPROM
    uint8_t b0 = EEPROM.read(EEPROM_WRITE_COUNT_ADDR);
    uint8_t b1 = EEPROM.read(EEPROM_WRITE_COUNT_ADDR + 1);
    uint8_t b2 = EEPROM.read(EEPROM_WRITE_COUNT_ADDR + 2);
    uint8_t b3 = EEPROM.read(EEPROM_WRITE_COUNT_ADDR + 3);
    
    total_eeprom_writes = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    
    // Sanity check - if garbage, reset to 0
    if (total_eeprom_writes > EEPROM_MAX_LIFETIME_WRITES * 2) {
        total_eeprom_writes = 0;
    }
    
    Serial.printf("[EEPROM] Lifetime writes: %u / %u\n", 
                  total_eeprom_writes, EEPROM_MAX_LIFETIME_WRITES);
    
    if (total_eeprom_writes > EEPROM_MAX_LIFETIME_WRITES * 0.9) {
        Serial.println("[EEPROM] ⚠️ WARNING: EEPROM nearing end of life!");
        eeprom_write_allowed = false;
    }
    
    hour_start_time = millis();
}

void save_eeprom_write_count() {
    EEPROM.write(EEPROM_WRITE_COUNT_ADDR, total_eeprom_writes & 0xFF);
    EEPROM.write(EEPROM_WRITE_COUNT_ADDR + 1, (total_eeprom_writes >> 8) & 0xFF);
    EEPROM.write(EEPROM_WRITE_COUNT_ADDR + 2, (total_eeprom_writes >> 16) & 0xFF);
    EEPROM.write(EEPROM_WRITE_COUNT_ADDR + 3, (total_eeprom_writes >> 24) & 0xFF);
}

bool can_save_eeprom() {
    // Check if EEPROM is near end of life
    if (!eeprom_write_allowed) {
        Serial.println("[EEPROM] ❌ Writes disabled - EEPROM worn out");
        return false;
    }
    
    // Check minimum time between saves
    if (millis() - last_eeprom_save_time < EEPROM_MIN_SAVE_INTERVAL) {
        Serial.println("[EEPROM] ⏳ Too soon since last save");
        return false;
    }
    
    // Reset hourly counter
    if (millis() - hour_start_time > 3600000) {  // 1 hour
        eeprom_saves_this_hour = 0;
        hour_start_time = millis();
    }
    
    // Check hourly limit
    if (eeprom_saves_this_hour >= EEPROM_MAX_SAVES_PER_HOUR) {
        Serial.println("[EEPROM] ❌ Hourly save limit reached");
        return false;
    }
    
    return true;
}

void eeprom_load() {
    EEPROM.begin(EEPROM_SIZE);
    
    load_eeprom_write_count();
    
    if (EEPROM.read(0) != EEPROM_MAGIC || EEPROM.read(1) != EEPROM_VERSION) {
        Serial.println("[EEPROM] Fresh start");
        seenCount = 0;
        seenIndex = 0;
        return;
    }
    
    seenCount = EEPROM.read(2) | (EEPROM.read(3) << 8);
    seenIndex = EEPROM.read(4) | (EEPROM.read(5) << 8);
    
    // Sanity checks
    if (seenCount > MAX_EVENTS) seenCount = MAX_EVENTS;
    if (seenIndex >= MAX_EVENTS) seenIndex = 0;
    
    int addr = 6;
    for (int i = 0; i < seenCount; i++) {
        for (int j = 0; j < ID_LENGTH; j++) {
            seenEvents[i][j] = EEPROM.read(addr++);
        }
    }
    
    Serial.printf("[EEPROM] Loaded %d seen events\n", seenCount);
}

void eeprom_save() {
    // *** PROTECTION: Check if we can save ***
    if (!can_save_eeprom()) {
        return;
    }
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, EEPROM_MAGIC);
    EEPROM.write(1, EEPROM_VERSION);
    EEPROM.write(2, seenCount & 0xFF);
    EEPROM.write(3, (seenCount >> 8) & 0xFF);
    EEPROM.write(4, seenIndex & 0xFF);
    EEPROM.write(5, (seenIndex >> 8) & 0xFF);
    
    int addr = 6;
    for (int i = 0; i < seenCount; i++) {
        for (int j = 0; j < ID_LENGTH; j++) {
            EEPROM.write(addr++, seenEvents[i][j]);
        }
    }
    
    // Update write counters
    total_eeprom_writes++;
    eeprom_saves_this_hour++;
    save_eeprom_write_count();
    
    EEPROM.commit();
    
    last_eeprom_save_time = millis();
    Serial.printf("[EEPROM] Saved %d events (writes: %u)\n", seenCount, total_eeprom_writes);
}

void eeprom_clear() {
    if (!can_save_eeprom()) {
        Serial.println("[EEPROM] Cannot clear - save not allowed");
        // Still clear RAM
        seenCount = 0;
        seenIndex = 0;
        memset(seenEvents, 0, sizeof(seenEvents));
        return;
    }
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, 0x00);
    
    total_eeprom_writes++;
    save_eeprom_write_count();
    
    EEPROM.commit();
    
    seenCount = 0;
    seenIndex = 0;
    memset(seenEvents, 0, sizeof(seenEvents));
    last_eeprom_save_time = millis();
    
    Serial.println("[EEPROM] Cleared");
}

// ==================== UART PROTECTION FUNCTIONS ====================

bool is_printable_message(const char* msg, int len) {
    if (len < UART_MSG_MIN_LEN || len > UART_MSG_MAX_LEN) return false;
    
    int printable = 0;
    int garbage = 0;
    
    for (int i = 0; i < len; i++) {
        char c = msg[i];
        // Count printable ASCII characters
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
            printable++;
        } else {
            garbage++;
        }
    }
    
    // Message is valid if >80% printable characters
    return (printable * 100 / len) > 80;
}

void reset_uart_health() {
    uart_garbage_count = 0;
    uart_healthy = true;
    Serial.println("[UART] Health reset");
}

void check_uart_health() {
    // Reset garbage counter periodically
    if (millis() - last_garbage_reset > UART_GARBAGE_RESET_MS) {
        if (uart_garbage_count > 0) {
            Serial.printf("[UART] Garbage count was %d, resetting\n", uart_garbage_count);
        }
        uart_garbage_count = 0;
        last_garbage_reset = millis();
    }
}

void flush_uart_garbage() {
    int flushed = 0;
    while (Serial1.available() && flushed < 100) {
        Serial1.read();
        flushed++;
    }
    if (flushed > 0) {
        Serial.printf("[UART] Flushed %d garbage bytes\n", flushed);
    }
}

bool detect_reversed_uart() {
    // If we get lots of 0xFF or 0x00, cables might be reversed
    // or there's a baud rate mismatch
    static int ff_count = 0;
    static int zero_count = 0;
    
    while (Serial1.available()) {
        uint8_t c = Serial1.read();
        if (c == 0xFF) ff_count++;
        else if (c == 0x00) zero_count++;
        else {
            // Got a normal character, reset counters
            ff_count = 0;
            zero_count = 0;
            return false;
        }
    }
    
    // Too many 0xFF usually means TX/RX reversed or disconnected
    if (ff_count > 20) {
        Serial.println("[UART] ⚠️ Possible reversed TX/RX or disconnected!");
        ff_count = 0;
        return true;
    }
    
    // Too many 0x00 might mean baud rate mismatch
    if (zero_count > 20) {
        Serial.println("[UART] ⚠️ Possible baud rate mismatch!");
        zero_count = 0;
        return true;
    }
    
    return false;
}

// ==================== MESHTASTIC TX ====================

void sendToHeltec(const char* message) {
    if (!message || strlen(message) == 0) return;
    if (!uart_healthy) {
        Serial.println("[MESH] ❌ UART unhealthy, skipping send");
        return;
    }
    
    Serial.print("Bot> ");
    Serial.println(message);
    Serial1.println(message);
    delay(100);
}

void queueLoraMessage(const char* message) {
    if (loraQueueCount >= LORA_QUEUE_SIZE) {
        Serial.println("[LORA] Queue full, dropping");
        return;
    }
    strncpy(loraQueue[loraQueueCount], message, 79);
    loraQueue[loraQueueCount][79] = '\0';
    loraQueueCount++;
}

void flushLoraQueue() {
    if (loraQueueCount == 0) return;
    
    Serial.printf("[MESH] Flushing %d messages\n", loraQueueCount);
    delay(200);
    
    for (int i = 0; i < loraQueueCount; i++) {
        feed_watchdog();  // Keep watchdog happy during long operations
        sendToHeltec(loraQueue[i]);
        delay(500);
    }
    
    loraQueueCount = 0;
}

// ==================== BUTTONS ====================

void emergency_clear_and_reboot() {
    Serial.println("[EMERGENCY] Button hold - Clearing and rebooting!");
    
    eeprom_clear();
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("EMERGENCY", 120, 40, 4);
    tft.drawString("CLEAR", 120, 70, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Rebooting...", 120, 100, 2);
    
    delay(2000);
    ESP.restart();
}

void check_buttons() {
    // Button 1 hold detection
    if (digitalRead(BUTTON_1) == LOW) {
        if (button1_hold_start == 0) {
            button1_hold_start = millis();
        } else if (!button1_held && (millis() - button1_hold_start >= BUTTON_HOLD_TIME)) {
            button1_held = true;
            emergency_clear_and_reboot();
        }
    } else {
        if (button1_hold_start > 0 && !button1_held) {
            if (millis() - button1_hold_start < BUTTON_HOLD_TIME) {
                Serial.println("[BTN1] Short press - Test LoRa");
                sendToHeltec("TEST DisasterAlert");
            }
        }
        button1_hold_start = 0;
        button1_held = false;
    }
    
    // Button 2 hold detection
    if (digitalRead(BUTTON_2) == LOW) {
        if (button2_hold_start == 0) {
            button2_hold_start = millis();
        } else if (!button2_held && (millis() - button2_hold_start >= BUTTON_HOLD_TIME)) {
            button2_held = true;
            emergency_clear_and_reboot();
        }
    } else {
        if (button2_hold_start > 0 && !button2_held) {
            if (millis() - button2_hold_start < BUTTON_HOLD_TIME) {
                Serial.println("[BTN2] Short press - Refetch");
                // Don't clear EEPROM on short press - just refetch
                if (wifiConnected) {
                    lastFetchTime = 0;  // Force fetch on next loop
                }
            }
        }
        button2_hold_start = 0;
        button2_held = false;
    }
}

// ==================== DISPLAY FUNCTIONS ====================

void showStartup() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("DISASTER", 120, 50, 4);
    tft.drawString("ALERT", 120, 85, 4);
}

void showFetching() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("FETCHING", 120, 50, 4);
    tft.drawString("DATA...", 120, 85, 4);
}

void showNoAlerts() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 240, 135, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("MONITORING", 120, 50, 4);
    tft.setTextColor(TFT_DARKCYAN, TFT_BLACK);
    tft.drawString("NO ALERTS", 120, 85, 4);
    
    // Show memory status
    char mem[32];
    snprintf(mem, sizeof(mem), "Mem:%uK", ESP.getFreeHeap() / 1024);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(mem, 120, 120, 1);
}

void showConnecting(int dots) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("CONNECTING", 120, 50, 4);
    
    String d = "WIFI ";
    for (int i = 0; i < (dots % 5); i++) d += ".";
    tft.drawString(d, 120, 85, 4);
}

void showConnected() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("CONNECTED", 120, 50, 4);
    tft.drawString(WiFi.localIP().toString(), 120, 85, 4);
}

void showError(const char* msg) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 240, 135, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("ERROR", 120, 45, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(msg, 120, 85, 2);
}

void showAlert(DisasterEvent* evt) {
    tft.fillScreen(TFT_BLACK);
    
    // Use alert color based on level
    uint16_t c = getAlertColor(evt->alertLevel);
    
    // Top color bar
    tft.fillRect(0, 0, 240, 10, c);
    
    // Event type (QUAKE, CYCLONE, FIRE, etc.)
    const char* typeName = getEventTypeName(evt->type);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(c, TFT_BLACK);
    tft.drawString(typeName, 10, 20, 4);
    
    // Magnitude (only if > 0)
    if (evt->magnitude > 0) {
        char mag[16];
        sprintf(mag, "M%.1f", evt->magnitude);
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(mag, 230, 20, 4);
    }
    
    // Location
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 60);
    tft.setTextFont(2);
    tft.setTextSize(2);
    tft.setTextWrap(true, true);
    tft.print(evt->location);
}

// ==================== EVENT TRACKING ====================

bool isEventSeen(const char* id) {
    for (int i = 0; i < seenCount; i++) {
        if (strcmp(seenEvents[i], id) == 0) return true;
    }
    return false;
}

void markEventSeen(const char* id) {
    if (isEventSeen(id)) return;
    
    strncpy(seenEvents[seenIndex], id, ID_LENGTH - 1);
    seenEvents[seenIndex][ID_LENGTH - 1] = '\0';
    seenIndex = (seenIndex + 1) % MAX_EVENTS;
    if (seenCount < MAX_EVENTS) seenCount++;
    
    Serial.printf("[SEEN] %s (total:%d)\n", id, seenCount);
    
    // *** REDUCED SAVE FREQUENCY ***
    // Only save every 10 events instead of 5
    static int n = 0;
    if (++n >= 10) {
        eeprom_save();
        n = 0;
    }
}

bool addToQueue(DisasterEvent* evt) {
    if (isEventSeen(evt->id)) return false;
    
    if (queueCount >= 5) {
        queueHead = (queueHead + 1) % 5;
        queueCount--;
    }
    
    memcpy(&displayQueue[queueTail], evt, sizeof(DisasterEvent));
    queueTail = (queueTail + 1) % 5;
    queueCount++;
    
    const char* typeName = getEventTypeName(evt->type);
    Serial.printf("[QUEUE] %s %s (q:%d)\n", typeName, evt->location, queueCount);
    
    // Format LoRa message based on event type
    char msg[80];
    if (evt->magnitude > 0) {
        snprintf(msg, sizeof(msg), "%s M%.1f %s", typeName, evt->magnitude, evt->location);
    } else {
        snprintf(msg, sizeof(msg), "%s %s", typeName, evt->location);
    }
    queueLoraMessage(msg);
    
    return true;
}

bool getFromQueue(DisasterEvent* evt) {
    if (queueCount == 0) return false;
    
    memcpy(evt, &displayQueue[queueHead], sizeof(DisasterEvent));
    queueHead = (queueHead + 1) % 5;
    queueCount--;
    markEventSeen(evt->id);
    
    return true;
}

// ==================== FETCH ====================

int fetchUSGS() {
    // *** MEMORY CHECK BEFORE FETCH ***
    if (!is_memory_safe()) {
        Serial.println("[USGS] ❌ Skipping fetch - low memory");
        return 0;
    }
    
    Serial.println("[USGS] Fetching earthquakes...");
    Serial.printf("[MEM] Free: %u bytes\n", ESP.getFreeHeap());
    
    feed_watchdog();
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, USGS_URL);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    int newEvents = 0;
    Serial.printf("[USGS] HTTP %d\n", httpCode);
    
    feed_watchdog();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("[USGS] Received %d bytes\n", payload.length());
        
        // *** CHECK PAYLOAD SIZE ***
        if (payload.length() > MAX_JSON_SIZE) {
            Serial.println("[USGS] ❌ Payload too large, skipping");
            http.end();
            return 0;
        }
        
        feed_watchdog();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        // Free payload memory immediately
        payload = String();
        
        if (error) {
            Serial.printf("[USGS] JSON error: %s\n", error.c_str());
        } else {
            JsonArray features = doc["features"];
            Serial.printf("[USGS] Parsed %d quakes\n", features.size());
            
            int count = 0;
            for (JsonObject feature : features) {
                if (++count > 5) break;
                
                feed_watchdog();
                
                DisasterEvent evt;
                memset(&evt, 0, sizeof(evt));
                
                const char* id = feature["id"] | "unknown";
                snprintf(evt.id, sizeof(evt.id), "usgs_%s", id);
                strcpy(evt.type, "EQ");  // Earthquake
                
                JsonObject props = feature["properties"];
                evt.magnitude = props["mag"] | 0.0f;
                const char* place = props["place"] | "Unknown";
                const char* of = strstr(place, " of ");
                strncpy(evt.location, of ? (of + 4) : place, sizeof(evt.location) - 1);
                
                if (evt.magnitude >= 7.0) evt.alertLevel = 2;
                else if (evt.magnitude >= 5.5) evt.alertLevel = 1;
                else evt.alertLevel = 0;
                
                if (addToQueue(&evt)) newEvents++;
            }
        }
    } else {
        Serial.printf("[USGS] HTTP error: %d\n", httpCode);
    }
    
    http.end();
    feed_watchdog();
    
    Serial.printf("[USGS] %d new quakes\n", newEvents);
    return newEvents;
}

// ==================== FETCH GDACS (Multi-hazard) ====================

int fetchGDACS() {
    if (!is_memory_safe()) {
        Serial.println("[GDACS] ❌ Skipping - low memory");
        return 0;
    }
    
    Serial.println("[GDACS] Fetching disasters...");
    feed_watchdog();
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, GDACS_URL);
    http.setTimeout(15000);
    http.addHeader("Accept", "application/json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    int newEvents = 0;
    Serial.printf("[GDACS] HTTP %d\n", httpCode);
    
    feed_watchdog();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("[GDACS] Received %d bytes\n", payload.length());
        
        if (payload.length() > MAX_JSON_SIZE) {
            Serial.println("[GDACS] ❌ Payload too large");
            http.end();
            return 0;
        }
        
        feed_watchdog();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        payload = String();
        
        if (error) {
            Serial.printf("[GDACS] JSON error: %s\n", error.c_str());
        } else {
            JsonArray features = doc["features"];
            Serial.printf("[GDACS] Parsed %d events\n", features.size());
            
            int count = 0;
            for (JsonObject feature : features) {
                if (++count > 5) break;
                feed_watchdog();
                
                DisasterEvent evt;
                memset(&evt, 0, sizeof(evt));
                
                JsonObject props = feature["properties"];
                
                int eventId = props["eventid"] | 0;
                snprintf(evt.id, sizeof(evt.id), "gdacs_%d", eventId);
                
                const char* eventType = props["eventtype"] | "UNK";
                strncpy(evt.type, eventType, sizeof(evt.type) - 1);
                
                const char* name = props["name"] | "Unknown";
                const char* country = props["country"] | "";
                if (strlen(country) > 0) {
                    snprintf(evt.location, sizeof(evt.location), "%s, %s", name, country);
                } else {
                    strncpy(evt.location, name, sizeof(evt.location) - 1);
                }
                
                const char* alertLevel = props["alertlevel"] | "Green";
                if (strcmp(alertLevel, "Red") == 0) evt.alertLevel = 2;
                else if (strcmp(alertLevel, "Orange") == 0) evt.alertLevel = 1;
                else evt.alertLevel = 0;
                
                // Get severity/magnitude if available
                JsonObject severity = props["severitydata"];
                evt.magnitude = severity["severity"] | 0.0f;
                
                if (addToQueue(&evt)) newEvents++;
            }
        }
    } else {
        Serial.printf("[GDACS] HTTP error: %d\n", httpCode);
    }
    
    http.end();
    feed_watchdog();
    
    Serial.printf("[GDACS] %d new events\n", newEvents);
    return newEvents;
}

// ==================== FETCH NASA EONET (Fires, Storms, Volcanoes) ====================

int fetchEONET() {
    if (!is_memory_safe()) {
        Serial.println("[EONET] ❌ Skipping - low memory");
        return 0;
    }
    
    Serial.println("[EONET] Fetching NASA events...");
    feed_watchdog();
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, EONET_URL);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    int newEvents = 0;
    Serial.printf("[EONET] HTTP %d\n", httpCode);
    
    feed_watchdog();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("[EONET] Received %d bytes\n", payload.length());
        
        if (payload.length() > MAX_JSON_SIZE) {
            Serial.println("[EONET] ❌ Payload too large");
            http.end();
            return 0;
        }
        
        feed_watchdog();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        payload = String();
        
        if (error) {
            Serial.printf("[EONET] JSON error: %s\n", error.c_str());
        } else {
            JsonArray events = doc["events"];
            Serial.printf("[EONET] Parsed %d events\n", events.size());
            
            int count = 0;
            for (JsonObject event : events) {
                if (++count > 5) break;
                feed_watchdog();
                
                DisasterEvent evt;
                memset(&evt, 0, sizeof(evt));
                
                const char* id = event["id"] | "unknown";
                snprintf(evt.id, sizeof(evt.id), "eonet_%s", id);
                
                // Get category (fire, storm, volcano, etc.)
                JsonArray categories = event["categories"];
                if (categories.size() > 0) {
                    const char* catId = categories[0]["id"] | "unknown";
                    strncpy(evt.type, catId, sizeof(evt.type) - 1);
                } else {
                    strcpy(evt.type, "event");
                }
                
                const char* title = event["title"] | "Unknown Event";
                strncpy(evt.location, title, sizeof(evt.location) - 1);
                
                // EONET doesn't have magnitude, use 0
                evt.magnitude = 0;
                
                // Default to orange for active events
                evt.alertLevel = 1;
                
                if (addToQueue(&evt)) newEvents++;
            }
        }
    } else {
        Serial.printf("[EONET] HTTP error: %d\n", httpCode);
    }
    
    http.end();
    feed_watchdog();
    
    Serial.printf("[EONET] %d new events\n", newEvents);
    return newEvents;
}

// ==================== FETCH NOAA SPACE WEATHER ====================

int fetchSpaceWeather() {
    if (!is_memory_safe()) {
        Serial.println("[SPACE] ❌ Skipping - low memory");
        return 0;
    }
    
    Serial.println("[SPACE] Fetching space weather...");
    feed_watchdog();
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, NOAA_SPACE_URL);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    int newEvents = 0;
    Serial.printf("[SPACE] HTTP %d\n", httpCode);
    
    feed_watchdog();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("[SPACE] Received %d bytes\n", payload.length());
        
        if (payload.length() > MAX_JSON_SIZE) {
            Serial.println("[SPACE] ❌ Payload too large");
            http.end();
            return 0;
        }
        
        feed_watchdog();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        payload = String();
        
        if (error) {
            Serial.printf("[SPACE] JSON error: %s\n", error.c_str());
        } else {
            JsonArray alerts = doc.as<JsonArray>();
            Serial.printf("[SPACE] Parsed %d alerts\n", alerts.size());
            
            int count = 0;
            for (JsonObject alert : alerts) {
                if (++count > 3) break;  // Limit space weather alerts
                feed_watchdog();
                
                DisasterEvent evt;
                memset(&evt, 0, sizeof(evt));
                
                // Get product code (ALTK30 = geomag, ALTTP2 = solar radiation, etc.)
                const char* productId = alert["product_id"] | "unknown";
                snprintf(evt.id, sizeof(evt.id), "noaa_%s", productId);
                
                // Determine event type from message
                const char* message = alert["message"] | "";
                
                if (strstr(message, "Geomagnetic") != NULL || strstr(message, "geomagnetic") != NULL) {
                    strcpy(evt.type, "GEOMAG");
                } else if (strstr(message, "Solar Flare") != NULL || strstr(message, "X-ray") != NULL) {
                    strcpy(evt.type, "FLARE");
                } else if (strstr(message, "CME") != NULL || strstr(message, "Coronal") != NULL) {
                    strcpy(evt.type, "CME");
                } else if (strstr(message, "Radio") != NULL || strstr(message, "Blackout") != NULL) {
                    strcpy(evt.type, "RADIO");
                } else {
                    strcpy(evt.type, "SOLAR");
                }
                
                // Extract a short description
                // NOAA messages are long, grab first ~50 chars
                strncpy(evt.location, message, 50);
                evt.location[50] = '\0';
                // Clean up - remove newlines
                for (int i = 0; evt.location[i]; i++) {
                    if (evt.location[i] == '\n' || evt.location[i] == '\r') {
                        evt.location[i] = ' ';
                    }
                }
                
                // Check for severity keywords
                if (strstr(message, "Extreme") != NULL || strstr(message, "G5") != NULL || 
                    strstr(message, "X") != NULL) {
                    evt.alertLevel = 2;  // Red
                    evt.magnitude = 5.0;
                } else if (strstr(message, "Severe") != NULL || strstr(message, "G4") != NULL ||
                           strstr(message, "Strong") != NULL || strstr(message, "G3") != NULL) {
                    evt.alertLevel = 1;  // Orange
                    evt.magnitude = 3.0;
                } else {
                    evt.alertLevel = 0;  // Green
                    evt.magnitude = 1.0;
                }
                
                if (addToQueue(&evt)) newEvents++;
            }
        }
    } else {
        Serial.printf("[SPACE] HTTP error: %d\n", httpCode);
    }
    
    http.end();
    feed_watchdog();
    
    Serial.printf("[SPACE] %d new alerts\n", newEvents);
    return newEvents;
}

// ==================== FETCH ALL SOURCES ====================

int fetchAllDisasters() {
    loraQueueCount = 0;  // Clear LoRa queue before fetching
    
    int total = 0;
    
    // Earthquakes
    total += fetchUSGS();
    delay(1000);
    feed_watchdog();
    
    // Multi-hazard (cyclones, floods, volcanoes, etc.)
    total += fetchGDACS();
    delay(1000);
    feed_watchdog();
    
    // NASA events (fires, storms, volcanoes)
    total += fetchEONET();
    delay(1000);
    feed_watchdog();
    
    // Space weather (solar flares, geomagnetic storms)
    total += fetchSpaceWeather();
    
    // Flush all LoRa messages at once
    flushLoraQueue();
    
    Serial.printf("[FETCH] Total: %d new events\n", total);
    Serial.printf("[MEM] Free after all: %u bytes\n", ESP.getFreeHeap());
    
    return total;
}

// ==================== MESH CHAT (PROTECTED) ====================

void monitor_mesh_chat() {
    // Check UART health periodically
    check_uart_health();
    
    // Check for reversed cables / noise
    if (detect_reversed_uart()) {
        uart_garbage_count += 10;
        if (uart_garbage_count > UART_NOISE_THRESHOLD * 3) {
            uart_healthy = false;
            Serial.println("[UART] ❌ Too much noise - disabling until reset");
            flush_uart_garbage();
        }
        return;
    }
    
    if (!Serial1.available()) return;
    
    feed_watchdog();
    
    // Read with timeout protection
    String incomingChat = "";
    unsigned long startRead = millis();
    
    while (millis() - startRead < UART_TIMEOUT_MS) {
        if (Serial1.available()) {
            char c = Serial1.read();
            
            // Stop at newline
            if (c == '\n') break;
            
            // Skip carriage return
            if (c == '\r') continue;
            
            // Protect against buffer overflow
            if (incomingChat.length() >= UART_MSG_MAX_LEN) {
                Serial.println("[UART] ⚠️ Message too long, truncating");
                break;
            }
            
            incomingChat += c;
            startRead = millis();  // Reset timeout on each char
        }
    }
    
    incomingChat.trim();
    
    // Validate the message
    if (incomingChat.length() == 0) return;
    
    // Check if message is valid (mostly printable characters)
    if (!is_printable_message(incomingChat.c_str(), incomingChat.length())) {
        uart_garbage_count++;
        Serial.printf("[UART] ⚠️ Garbage detected (%d/%d): ", 
                      uart_garbage_count, UART_NOISE_THRESHOLD);
        
        // Print as hex for debugging
        for (int i = 0; i < min((int)incomingChat.length(), 10); i++) {
            Serial.printf("%02X ", (uint8_t)incomingChat[i]);
        }
        Serial.println();
        
        if (uart_garbage_count >= UART_NOISE_THRESHOLD) {
            Serial.println("[UART] ❌ Too much garbage - flushing buffer");
            flush_uart_garbage();
            uart_garbage_count = 0;
        }
        return;
    }
    
    // Valid message received!
    uart_garbage_count = 0;  // Reset on good message
    
    Serial.print("[MESH RX]: ");
    Serial.println(incomingChat);
    
    display_mesh_chat(incomingChat.c_str());
    
    // Shorter delay with watchdog feeding
    for (int i = 0; i < 50; i++) {
        delay(100);
        feed_watchdog();
    }
    
    lastDisplayChange = 0;
}

// ==================== SETUP ====================

void setup() {
    // *** DISABLE BROWN-OUT DETECTOR (prevents random resets) ***
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    Serial.begin(115200);
    delay(100);
    
    // Button setup
    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT_PULLUP);
    
    // Init TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    // Backlight with PWM
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL_PIN, 0);
    ledcWrite(0, 255);
    
    Serial.println("\n=================================");
    Serial.println("  Disaster Alert v2.3 PROTECTED");
    Serial.println("  EEPROM wear + Memory safety");
    Serial.println("=================================\n");
    
    // *** INIT WATCHDOG ***
    init_watchdog();
    
    // Init UART for Meshtastic
    Serial1.begin(MESH_BAUD, SERIAL_8N1, MESH_RX_PIN, MESH_TX_PIN);
    Serial.printf("[MESH] TX:%d RX:%d %dbaud\n", MESH_TX_PIN, MESH_RX_PIN, MESH_BAUD);
    
    eeprom_load();
    showStartup();
    delay(2000);
    
    feed_watchdog();
    
    sendToHeltec("DisasterAlert v2.3 online");
    
    Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long start = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        feed_watchdog();
        Serial.printf("[WIFI] Status:%d\n", WiFi.status());
        showConnecting(dots++);
        delay(500);
        
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("[WIFI] Timeout!");
            showError("WIFI FAIL");
            delay(5000);
            break;
        }
    }
    
    feed_watchdog();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());
        showConnected();
        delay(1500);
        
        showFetching();
        delay(500);
        
        int n = fetchAllDisasters();
        Serial.printf("[FETCH] %d new events\n", n);
        lastFetchTime = millis();
    }
    
    Serial.println("[MAIN] System Ready");
}

// ==================== LOOP ====================

void loop() {
    // *** FEED WATCHDOG EVERY LOOP ***
    feed_watchdog();
    
    // Check buttons
    for (int i = 0; i < 20; i++) {
        check_buttons();
        delay(1);
    }
    
    // *** CHECK MEMORY ***
    check_memory();
    
    // Check serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'C' || cmd == 'c') {
            Serial.println("[CMD] Clear");
            eeprom_clear();
            if (wifiConnected) {
                fetchAllDisasters();
                lastFetchTime = millis();
            }
        }
        if (cmd == 'T' || cmd == 't') {
            Serial.println("[CMD] Test LoRa");
            sendToHeltec("TEST DisasterAlert");
        }
        if (cmd == 'M' || cmd == 'm') {
            Serial.printf("[CMD] Memory: %u free, %u min\n", 
                          ESP.getFreeHeap(), ESP.getMinFreeHeap());
        }
        if (cmd == 'E' || cmd == 'e') {
            Serial.printf("[CMD] EEPROM writes: %u\n", total_eeprom_writes);
        }
        if (cmd == 'U' || cmd == 'u') {
            Serial.println("[CMD] UART reset");
            reset_uart_health();
            flush_uart_garbage();
        }
        if (cmd == 'H' || cmd == 'h' || cmd == '?') {
            Serial.println("\n=== COMMANDS ===");
            Serial.println("C = Clear EEPROM & refetch");
            Serial.println("T = Test LoRa TX");
            Serial.println("M = Memory status");
            Serial.println("E = EEPROM write count");
            Serial.println("U = Reset UART health");
            Serial.println("H = This help\n");
        }
    }
    
    // Monitor mesh chat
    monitor_mesh_chat();
    
    // Monitor WiFi
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        Serial.println("[WIFI] Lost!");
        showError("WIFI LOST");
        delay(3000);
    }
    
    // Periodic fetch (only if memory is safe)
    if (wifiConnected && is_memory_safe() && 
        (millis() - lastFetchTime >= FETCH_INTERVAL_MS)) {
        Serial.println("[FETCH] Periodic check...");
        showFetching();
        delay(500);
        fetchAllDisasters();
        lastFetchTime = millis();
    }
    
    // Update display
    unsigned long now = millis();
    if (queueCount > 0) {
        if (!showingAlert || (now - lastDisplayChange >= DISPLAY_DURATION_MS)) {
            if (getFromQueue(&currentEvent)) {
                showAlert(&currentEvent);
                showingAlert = true;
                lastDisplayChange = now;
                Serial.printf("[DISPLAY] M%.1f %s\n", 
                              currentEvent.magnitude, currentEvent.location);
            }
        }
    } else {
        if (showingAlert || (now - lastDisplayChange >= 5000)) {
            showNoAlerts();
            showingAlert = false;
            lastDisplayChange = now;
        }
    }
}