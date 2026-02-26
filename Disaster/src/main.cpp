/*
 * ESP32 TTGO T-Display Disaster Alert v2.2
 * Fixed JSON Parsing for ArduinoJson v7
 */

 
 
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>

 
extern void display_mesh_chat(const char* message) ;

// ==================== WIFI ====================
const char* WIFI_SSID     = "demonoid";
const char* WIFI_PASSWORD = "lacasadepapel2019";

// ==================== TIMING ====================
#define FETCH_INTERVAL_MS   (5UL * 60UL * 1000UL)
#define DISPLAY_DURATION_MS (8UL * 1000UL)
#define WIFI_TIMEOUT_MS     30000

// ==================== TTGO HARDWARE ====================
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL_PIN 4     // TTGO T-Display Backlight
#define BUTTON_1   35    // Right button (near USB usually)
#define BUTTON_2   0     // Left button

// ==================== MESHTASTIC ====================
#define MESH_TX_PIN 27    // TTGO GPIO 27 (TX) -> Heltec pin 48 (RX)
#define MESH_RX_PIN 25    // TTGO GPIO 25 (RX) <- Heltec pin 47 (TX)
#define MESH_BAUD   9600

// ==================== API ====================
// Note: If you want to force-test with lots of quakes, change 4.5 to 2.5
const char* USGS_URL = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson";

// ==================== EEPROM ====================
#define EEPROM_SIZE    512
#define EEPROM_MAGIC   0xDA
#define EEPROM_VERSION 0x01
#define MAX_EVENTS     20
#define ID_LENGTH      24

#define EEPROM_SAVE_INTERVAL    5
#define MAX_JSON_SIZE           16384  // 16KB max for JSON
#define STACK_MONITOR_INTERVAL  10000  // Check stack every 10 seconds
#define BUTTON_HOLD_TIME        3000   // 3 seconds hold to clear
#define EEPROM_SAVE_INTERVAL    5
#define EMERGENCY_REBOOT_DELAY  3000 

char seenEvents[MAX_EVENTS][ID_LENGTH];
int  seenCount = 0;
int  seenIndex = 0;

// ==================== QUEUE ====================
struct DisasterEvent {
    char    id[ID_LENGTH];
    char    location[64]; 
    float   magnitude;
    uint8_t alertLevel;
};

DisasterEvent displayQueue[5];
int queueHead  = 0;
int queueTail  = 0;
int queueCount = 0;


void monitor_mesh_chat();
void check_memory();
void check_buttons() ;
void emergency_clear_and_reboot();

extern void eeprom_clear(void);
extern bool wifiConnected;
extern int fetchUSGS(void);
extern unsigned long lastFetchTime;


static unsigned long button1_hold_start = 0;
static unsigned long button2_hold_start = 0;
static bool button1_held = false;
static bool button2_held = false;
static bool emergency_clear_pending = false;

// ==================== LORA PENDING QUEUE ====================
#define LORA_QUEUE_SIZE 5
char loraQueue[LORA_QUEUE_SIZE][80];
int  loraQueueCount = 0;

// ==================== MESHTASTIC TX ====================

void sendToHeltec(const char* message) {
    if (!message || strlen(message) == 0) return;
    Serial.print("Bot> ");
    Serial.println(message);
    Serial1.println(message);
    delay(100); 
}





void emergency_clear_and_reboot() {
    Serial.println("[EMERGENCY] Button hold detected - Clearing EEPROM and rebooting!");
    
    // Clear all data
    eeprom_clear();
    
    // Show message on screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("EMERGENCY", 120, 40, 4);
    tft.drawString("CLEAR", 120, 70, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Rebooting...", 120, 100, 2);
    
    delay(3000);
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
        // Short press handling
        if (button1_hold_start > 0 && !button1_held) {
            if (millis() - button1_hold_start < BUTTON_HOLD_TIME) {
                Serial.println("[DEBUG] Button 1 short press - Testing LoRa");
                sendToHeltec("TEST DisasterAlert TTGO->Heltec");
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
        // Short press handling
        if (button2_hold_start > 0 && !button2_held) {
            if (millis() - button2_hold_start < BUTTON_HOLD_TIME) {
                Serial.println("[DEBUG] Button 2 short press - Clearing & Refetching");
                eeprom_clear();
                if (wifiConnected) { 
                    fetchUSGS(); 
                    lastFetchTime = millis(); 
                }
            }
        }
        button2_hold_start = 0;
        button2_held = false;
    }
}


void check_memory() {
    static unsigned long last_memory_check = 0;
    if (millis() - last_memory_check > STACK_MONITOR_INTERVAL) {
        last_memory_check = millis();
        
        uint32_t free_heap = ESP.getFreeHeap();
        uint32_t min_free_heap = ESP.getMinFreeHeap();
        uint32_t max_alloc = ESP.getMaxAllocHeap();
        
        Serial.printf("[MEMORY] Free: %u, Min: %u, Max Alloc: %u\n", 
                      free_heap, min_free_heap, max_alloc);
        
        if (free_heap < 10000) {
            Serial.println("[WARNING] Low memory!");
            // Force garbage collection
            delay(10);
        }
    }
}

void queueLoraMessage(const char* message) {
    if (loraQueueCount >= LORA_QUEUE_SIZE) {
        Serial.println("[DEBUG] LoRa Queue Full! Dropping message.");
        return;
    }
    strncpy(loraQueue[loraQueueCount], message, 79);
    loraQueue[loraQueueCount][79] = '\0';
    loraQueueCount++;
    Serial.printf("[DEBUG] Queued LoRa msg %d/%d\n", loraQueueCount, LORA_QUEUE_SIZE);
}

void flushLoraQueue() {
    if (loraQueueCount == 0) return;
    Serial.printf("[MESH] Flushing %d queued messages...\n", loraQueueCount);
    delay(200);  
    for (int i = 0; i < loraQueueCount; i++) {
        sendToHeltec(loraQueue[i]);
        delay(500); 
    }
    loraQueueCount = 0;
    Serial.println("[DEBUG] LoRa Queue flushed entirely.");
}

// ==================== EEPROM ====================

void eeprom_load() {
    EEPROM.begin(EEPROM_SIZE);
    if (EEPROM.read(0) != EEPROM_MAGIC || EEPROM.read(1) != EEPROM_VERSION) {
        Serial.println("[EEPROM] Fresh start");
        seenCount = 0; seenIndex = 0; return;
    }
    seenCount = EEPROM.read(2) | (EEPROM.read(3) << 8);
    seenIndex = EEPROM.read(4) | (EEPROM.read(5) << 8);
    if (seenCount > MAX_EVENTS) seenCount = MAX_EVENTS;
    if (seenIndex >= MAX_EVENTS) seenIndex = 0;
    int addr = 6;
    for (int i=0;i<seenCount;i++)
        for (int j=0;j<ID_LENGTH;j++)
            seenEvents[i][j] = EEPROM.read(addr++);
    Serial.printf("[EEPROM] Loaded %d seen events\n", seenCount);
}

void eeprom_save() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, EEPROM_MAGIC);  EEPROM.write(1, EEPROM_VERSION);
    EEPROM.write(2, seenCount&0xFF); EEPROM.write(3,(seenCount>>8)&0xFF);
    EEPROM.write(4, seenIndex&0xFF); EEPROM.write(5,(seenIndex>>8)&0xFF);
    int addr = 6;
    for (int i=0;i<seenCount;i++)
        for (int j=0;j<ID_LENGTH;j++)
            EEPROM.write(addr++, seenEvents[i][j]);
    EEPROM.commit();
    Serial.printf("[EEPROM] Saved %d events\n", seenCount);
}

void eeprom_clear() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, 0x00);
    EEPROM.commit();
    seenCount = 0; seenIndex = 0;
    memset(seenEvents, 0, sizeof(seenEvents));
    Serial.println("[EEPROM] Cleared — all quakes will re-queue");
}

// ==================== SCREENS ====================
void showStartup()  { 
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Middle center
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
}

void showConnecting(int dots) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("CONNECTING", 120, 50, 4);
    
    String d = "WIFI ";
    for(int i=0; i<(dots%5); i++) d += ".";
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
    uint16_t c = TFT_GREEN;
    if(evt->alertLevel == 2) c = TFT_RED;
    else if(evt->alertLevel == 1) c = TFT_ORANGE;
    
    tft.fillRect(0, 0, 240, 10, c);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(c, TFT_BLACK);
    tft.drawString("QUAKE", 10, 20, 4); 

    char mag[16]; 
    sprintf(mag, "M%.1f", evt->magnitude);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(mag, 230, 20, 4);

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
    for(int i=0;i<seenCount;i++) if(strcmp(seenEvents[i],id)==0) return true;
    return false;
}

void markEventSeen(const char* id) {
    if(isEventSeen(id)) return;
    strncpy(seenEvents[seenIndex],id,ID_LENGTH-1);
    seenEvents[seenIndex][ID_LENGTH-1]='\0';
    seenIndex=(seenIndex+1)%MAX_EVENTS;
    if(seenCount<MAX_EVENTS) seenCount++;
    Serial.printf("[SEEN] %s (total:%d)\n",id,seenCount);
    static int n=0; if(++n>=5){eeprom_save();n=0;}
}

bool addToQueue(DisasterEvent* evt) {
    if(isEventSeen(evt->id)) return false;
    if(queueCount>=5){queueHead=(queueHead+1)%5;queueCount--;}
    memcpy(&displayQueue[queueTail],evt,sizeof(DisasterEvent));
    queueTail=(queueTail+1)%5; queueCount++;
    Serial.printf("[QUEUE] M%.1f %s (q:%d)\n",evt->magnitude,evt->location,queueCount);

    char msg[80];
    snprintf(msg,sizeof(msg),"QUAKE M%.1f %s",evt->magnitude,evt->location);
    queueLoraMessage(msg);
    return true;
}

bool getFromQueue(DisasterEvent* evt) {
    if(queueCount==0) return false;
    memcpy(evt,&displayQueue[queueHead],sizeof(DisasterEvent));
    queueHead=(queueHead+1)%5; queueCount--;
    markEventSeen(evt->id);
    return true;
}

// ==================== FETCH ====================

int fetchUSGS() {
    Serial.println("[USGS] Fetching...");
    Serial.printf("[DEBUG] Free Heap before fetch: %u bytes\n", ESP.getFreeHeap());
    loraQueueCount = 0;  // Clear LoRa staging buffer

    WiFiClientSecure client; 
    client.setInsecure();
    HTTPClient http;
    http.begin(client, USGS_URL); 
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
    int newEvents = 0;
    Serial.printf("[USGS] HTTP %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
        
        // --- FIXED: Read the raw string instead of memory-crashing stream filter ---
        String payload = http.getString();
        Serial.printf("[DEBUG] Received %d bytes from API\n", payload.length());

        JsonDocument doc;  // ArduinoJson v7 syntax
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.printf("[USGS] JSON error: %s\n", error.c_str());
        } else {
            JsonArray features = doc["features"];
            Serial.printf("[USGS] Parsed %d quakes\n", features.size());

            int count = 0;
            for (JsonObject feature : features) {
                if (++count > 5) break;

                DisasterEvent evt; memset(&evt, 0, sizeof(evt));
                
                const char* id = feature["id"] | "unknown";
                snprintf(evt.id, sizeof(evt.id), "usgs_%s", id);
                
                JsonObject props = feature["properties"];
                evt.magnitude = props["mag"] | 0.0f;
                const char* place = props["place"] | "Unknown";
                const char* of = strstr(place, " of ");
                strncpy(evt.location, of ? (of + 4) : place, sizeof(evt.location) - 1);
                
                if (evt.magnitude >= 7.0)      evt.alertLevel = 2;
                else if (evt.magnitude >= 5.5) evt.alertLevel = 1;
                else                           evt.alertLevel = 0;
                
                if (addToQueue(&evt)) newEvents++;
            }
        }
    } else {
        Serial.printf("[USGS] HTTP error: %d\n", httpCode);
    }

    http.end();  

    flushLoraQueue();

    Serial.printf("[USGS] %d new events fetched.\n", newEvents);
    Serial.printf("[DEBUG] Free Heap after fetch: %u bytes\n", ESP.getFreeHeap());
    return newEvents;
}

// ==================== GLOBALS ====================
unsigned long lastFetchTime     = 0;
unsigned long lastDisplayChange = 0;
bool          wifiConnected     = false;
DisasterEvent currentEvent;
bool          showingAlert      = false;

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    
    // Key/Button setup
    pinMode(BUTTON_1, INPUT);        // GPIO35 usually has external pullup on TTGO
    pinMode(BUTTON_2, INPUT_PULLUP); // GPIO0 needs internal pullup
    
    // Init TFT
    tft.init();
    tft.setRotation(1); // Landscape (240x135)
    tft.fillScreen(TFT_BLACK);
    
    // Turn on TFT Backlight manually if needed
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    Serial.println("\n=================================");
    Serial.println("  Disaster Alert Display v2.2");
    Serial.println("  ESP32 TTGO T-Display + LoRa");
    Serial.println("=================================");
    Serial.println("  UART / Key Commands:");
    Serial.println("  Button 1 / T = Test LoRa message");
    Serial.println("  Button 2 / C = Clear EEPROM");
    Serial.println("=================================\n");

    // Initialize UART 1 for Meshtastic
    Serial1.begin(MESH_BAUD, SERIAL_8N1, MESH_RX_PIN, MESH_TX_PIN);
    Serial.printf("[MESH] GPIO%d->Heltec(RX)  GPIO%d<-Heltec(TX)  %dbaud\n", MESH_TX_PIN, MESH_RX_PIN, MESH_BAUD);

    eeprom_load();
    showStartup();
    delay(2000);

    sendToHeltec("DisasterAlert v2.2 online");

    Serial.printf("[WIFI] Connecting to %s\n",WIFI_SSID);
    WiFi.mode(WIFI_STA); 
    WiFi.disconnect(); 
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start=millis(); 
    int dots=0;
    while(WiFi.status()!=WL_CONNECTED){
        Serial.printf("[WIFI] Status:%d\n",WiFi.status());
        showConnecting(dots++);
        delay(500);
        
        if(millis()-start>WIFI_TIMEOUT_MS){
            Serial.println("[WIFI] Timeout!");
            showError("WIFI FAIL");
            delay(5000);
            break;
        }
    }

    if(WiFi.status()==WL_CONNECTED){
        wifiConnected=true;
        Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
        showConnected(); 
        delay(1500);
        
        showFetching();  
        delay(500);
        
        int n=fetchUSGS();
        Serial.printf("[FETCH] %d new events\n",n);
        lastFetchTime=millis();
    }
    Serial.println("[MAIN] System Ready.");
}

// ==================== LOOP ====================

void loop() {


    
for(int i=0;i<20;i++) {
     check_buttons();
    delay(1);
}



    // 1. Check Physical Buttons
    static unsigned long lastBtn1 = 0;
    if (digitalRead(BUTTON_1) == LOW) {
        if (millis() - lastBtn1 > 500) {
            Serial.println("[DEBUG] Key 1 pressed - Testing LoRa");
            sendToHeltec("TEST DisasterAlert TTGO->Heltec");
            lastBtn1 = millis();
        }
    }

    static unsigned long lastBtn2 = 0;
    if (digitalRead(BUTTON_2) == LOW) {
        if (millis() - lastBtn2 > 500) {
            Serial.println("[DEBUG] Key 2 pressed - Clearing EEPROM & Re-Fetching");
            eeprom_clear();
            if (wifiConnected) { fetchUSGS(); lastFetchTime = millis(); }
            lastBtn2 = millis();
        }
    }

    // 2. Check UART Serial commands
    if(Serial.available()){
        char cmd=Serial.read();
        if(cmd=='C'||cmd=='c'){
            Serial.println("[DEBUG] UART 'C' received");
            eeprom_clear();
            if(wifiConnected){ fetchUSGS(); lastFetchTime=millis(); }
        }
        if(cmd=='T'||cmd=='t'){
            Serial.println("[DEBUG] UART 'T' received");
            sendToHeltec("TEST DisasterAlert TTGO->Heltec");
        }
    }

   // 2.5  monitor incoming chat
    monitor_mesh_chat();

    // 3. Monitor WiFi
    if(wifiConnected && WiFi.status()!=WL_CONNECTED){
        wifiConnected=false; 
        Serial.println("[WIFI] Connection Lost!");
        showError("WIFI LOST");
        delay(3000);
    }

    // 4. Periodic API Fetch
    if(wifiConnected && (millis()-lastFetchTime>=FETCH_INTERVAL_MS)){
        Serial.println("[FETCH] Periodic API Check...");
        showFetching(); 
        delay(500);
        fetchUSGS(); 
        lastFetchTime=millis();
    }

    // 5. Update Screen with Alerts / Status
    unsigned long now=millis();
    if(queueCount>0){
        if(!showingAlert||(now-lastDisplayChange>=DISPLAY_DURATION_MS)){
            if(getFromQueue(&currentEvent)){
                showAlert(&currentEvent);
                showingAlert=true; lastDisplayChange=now;
                Serial.printf("[DISPLAY] Showing Alert: M%.1f %s\n",
                              currentEvent.magnitude,currentEvent.location);
            }
        }
    } else {
        if(showingAlert||(now-lastDisplayChange>=5000)){
            showNoAlerts(); 
            showingAlert=false; 
            lastDisplayChange=now;
        }
    }
}



// ==================== MESH CHAT MONITOR ====================
void monitor_mesh_chat() {
    // Check if the Heltec has sent any text over the RX pin
    if (Serial1.available()) { 
        String incomingChat = Serial1.readStringUntil('\n');
        incomingChat.trim(); // Remove extra spaces or hidden characters
        
        // If the message isn't empty, display it!
        if (incomingChat.length() > 0) {
            Serial.print("[MESH CHAT RECEIVED]: ");
            Serial.println(incomingChat);
            
            // 1. Show it on the TTGO screen (Make sure you added this to display_renderer!)
            display_mesh_chat(incomingChat.c_str());
            
            // 2. Pause for 5 seconds so you have time to read it
            delay(5000); 
            
            // 3. Force the screen to refresh back to normal alerts immediately after
            // (This uses the variables already at the top of your main.cpp)
            extern unsigned long lastDisplayChange; 
            lastDisplayChange = 0; 
        }
    }
}







