/*
 * json_parser.cpp - HTTP Fetcher and JSON Parser Implementation
 */

#include "json_parser.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ====== LORA EXPORTS ======
extern void queueLoraMessage(const char* message);
extern void flushLoraQueue();

// ==================== Memory Protection ====================
static unsigned long last_http_request = 0;
static const unsigned long HTTP_RATE_LIMIT = 2000; // 2 seconds between requests

void sanitize_for_lora(char* str) {
    if (!str) return;
    
    int read_idx = 0;
    int write_idx = 0;
    size_t len = strlen(str);
    
    // Prevent buffer overflow
    if (len > 256) len = 256;
    
    while (str[read_idx] != '\0' && read_idx < len) {
        unsigned char c = str[read_idx];
        unsigned char next_c = str[read_idx + 1];
        
        // UTF-8 handling
        if (c == 0xC3 && next_c != '\0') {
            char replacement = 0;
            
            switch (next_c) {
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: replacement = 'a'; break;
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: replacement = 'A'; break;
                case 0xA8: case 0xA9: case 0xAA: case 0xAB: replacement = 'e'; break;
                case 0x88: case 0x89: case 0x8A: case 0x8B: replacement = 'E'; break;
                case 0xAC: case 0xAD: case 0xAE: case 0xAF: replacement = 'i'; break;
                case 0x8C: case 0x8D: case 0x8E: case 0x8F: replacement = 'I'; break;
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: replacement = 'o'; break;
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: replacement = 'O'; break;
                case 0xB9: case 0xBA: case 0xBB: case 0xBC: replacement = 'u'; break;
                case 0x99: case 0x9A: case 0x9B: case 0x9C: replacement = 'U'; break;
                case 0xB1: replacement = 'n'; break;
                case 0x91: replacement = 'N'; break;
                case 0xA7: replacement = 'c'; break;
                case 0x87: replacement = 'C'; break;
            }
            
            if (replacement != 0 && write_idx < len - 1) {
                str[write_idx++] = replacement;
            }
            
            read_idx += 2; 
            continue; 
        }
        
        // ASCII filter
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '-') {
            
            if (write_idx < len - 1) {
                str[write_idx++] = c;
            }
        }
        
        read_idx++;
    }
    
    str[write_idx] = '\0'; 
}

// ==================== HTTP Functions ====================

void http_init(void) {
    Serial.println("[HTTP] HTTP client ready");
}

bool is_wifi_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

const char* get_wifi_status(void) {
    switch (WiFi.status()) {
        case WL_CONNECTED:    return "Connected";
        case WL_NO_SHIELD:    return "No WiFi";
        case WL_IDLE_STATUS:  return "Idle";
        case WL_NO_SSID_AVAIL: return "No SSID";
        case WL_SCAN_COMPLETED: return "Scan Done";
        case WL_CONNECT_FAILED: return "Failed";
        case WL_CONNECTION_LOST: return "Lost";
        case WL_DISCONNECTED: return "Disconnected";
        default:              return "Unknown";
    }
}

// ==================== USGS Parser ====================

int fetch_usgs_data(void) {
    if (!is_wifi_connected()) {
        Serial.println("[USGS] WiFi not connected");
        return 0;
    }
    
    // Rate limiting
    if (millis() - last_http_request < HTTP_RATE_LIMIT) {
        delay(HTTP_RATE_LIMIT - (millis() - last_http_request));
    }
    
    Serial.println("[USGS] Fetching earthquake data...");
    
    HTTPClient *http = new HTTPClient();
    if (!http) {
        Serial.println("[USGS] Failed to allocate HTTP client");
        return 0;
    }
    
    http->begin(USGS_URL);
    http->setTimeout(HTTP_TIMEOUT_MS);
    http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http->GET();
    int newEvents = 0;
    
    if (httpCode == HTTP_CODE_OK) {
        // Get raw stream to avoid large String allocation
        WiFiClient *stream = http->getStreamPtr();
        if (!stream) {
            Serial.println("[USGS] Failed to get stream");
            http->end();
            delete http;
            return 0;
        }
        
        // Use a smaller buffer and parse incrementally
        JsonDocument doc;
        
        // Check available memory before parsing
        if (ESP.getFreeHeap() < 20000) {
            Serial.println("[USGS] Low memory, skipping parse");
            http->end();
            delete http;
            return 0;
        }
        
        DeserializationError error = deserializeJson(doc, *stream);
        
        if (error) {
            Serial.printf("[USGS] JSON error: %s\n", error.c_str());
        } else {
            JsonArray features = doc["features"];
            Serial.printf("[USGS] Found %d earthquakes\n", features.size());
            
            int count = 0;
            for (JsonObject feature : features) {
                if (++count > MAX_DISPLAY_QUEUE) break;
                
                disaster_event_t event = {0};
                
                strcpy(event.type, "EQ");
                
                const char* id = feature["id"] | "";
                snprintf(event.id, sizeof(event.id), "usgs_%s", id);
                
                JsonObject props = feature["properties"];
                
                event.magnitude = props["mag"] | 0.0f;
                
                const char* place = props["place"] | "Unknown";
                strncpy(event.title, place, sizeof(event.title) - 1);
                
                const char* of = strstr(place, " of ");
                if (of) {
                    strncpy(event.country, of + 4, sizeof(event.country) - 1);
                } else {
                    strncpy(event.country, place, sizeof(event.country) - 1);
                }
                
                if (event.magnitude >= 7.0) event.alert_level = 2;
                else if (event.magnitude >= 5.5) event.alert_level = 1;
                else event.alert_level = 0;
                
                if (add_event_to_queue(&event)) {
                    newEvents++;
                    Serial.printf("[USGS] NEW: M%.1f - %s\n", 
                                  event.magnitude, event.title);
                    
                    char loraMsg[80];
                    snprintf(loraMsg, sizeof(loraMsg), "QUAKE M%.1f %s", 
                             event.magnitude, event.title);
                    sanitize_for_lora(loraMsg);
                    queueLoraMessage(loraMsg);
                }
            }
        }
    } else {
        Serial.printf("[USGS] HTTP error: %d\n", httpCode);
    }
    
    http->end();
    delete http; // Free memory
    last_http_request = millis();
    
    Serial.printf("[USGS] Found %d new events\n", newEvents);
    return newEvents;
}

int fetch_gdacs_data(void) {
    if (!is_wifi_connected()) {
        Serial.println("[GDACS] WiFi not connected");
        return 0;
    }
    
    if (millis() - last_http_request < HTTP_RATE_LIMIT) {
        delay(HTTP_RATE_LIMIT - (millis() - last_http_request));
    }
    
    Serial.println("[GDACS] Fetching disaster data...");
    
    HTTPClient *http = new HTTPClient();
    if (!http) {
        Serial.println("[GDACS] Failed to allocate HTTP client");
        return 0;
    }
    
    http->begin(GDACS_URL);
    http->setTimeout(HTTP_TIMEOUT_MS);
    http->addHeader("Accept", "application/json");
    http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http->GET();
    int newEvents = 0;
    
    if (httpCode == HTTP_CODE_OK) {
        WiFiClient *stream = http->getStreamPtr();
        if (!stream) {
            Serial.println("[GDACS] Failed to get stream");
            http->end();
            delete http;
            return 0;
        }
        
        if (ESP.getFreeHeap() < 20000) {
            Serial.println("[GDACS] Low memory, skipping parse");
            http->end();
            delete http;
            return 0;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *stream);
        
        if (error) {
            Serial.printf("[GDACS] JSON error: %s\n", error.c_str());
        } else {
            JsonArray features = doc["features"];
            Serial.printf("[GDACS] Found %d events\n", features.size());
            
            int count = 0;
            for (JsonObject feature : features) {
                if (++count > MAX_DISPLAY_QUEUE) break;
                
                disaster_event_t event = {0};
                
                JsonObject props = feature["properties"];
                
                int eventId = props["eventid"] | 0;
                snprintf(event.id, sizeof(event.id), "gdacs_%d", eventId);
                
                const char* eventType = props["eventtype"] | "UNK";
                strncpy(event.type, eventType, sizeof(event.type) - 1);
                
                const char* name = props["name"] | "Unknown Event";
                strncpy(event.title, name, sizeof(event.title) - 1);
                
                const char* country = props["country"] | "Unknown";
                strncpy(event.country, country, sizeof(event.country) - 1);
                
                const char* alertLevel = props["alertlevel"] | "Green";
                if (strcmp(alertLevel, "Red") == 0) event.alert_level = 2;
                else if (strcmp(alertLevel, "Orange") == 0) event.alert_level = 1;
                else event.alert_level = 0;
                
                JsonObject severity = props["severitydata"];
                event.magnitude = severity["severity"] | 0.0f;
                
                if (add_event_to_queue(&event)) {
                    newEvents++;
                    Serial.printf("[GDACS] NEW: %s - %s (%s)\n",
                                  event.type, event.title, event.country);
                    
                    char loraMsg[80];
                    snprintf(loraMsg, sizeof(loraMsg), "%s %s", 
                             event.type, event.title);
                    queueLoraMessage(loraMsg);
                }
            }
        }
    } else {
        Serial.printf("[GDACS] HTTP error: %d\n", httpCode);
    }
    
    http->end();
    delete http;
    last_http_request = millis();
    
    Serial.printf("[GDACS] Found %d new events\n", newEvents);
    return newEvents;
}

// ==================== Fetch All ====================

int fetch_all_data(void) {
    int total = 0;
    
    total += fetch_usgs_data();
    delay(2000);
    
    total += fetch_gdacs_data();
    
    Serial.printf("[FETCH] Total new events: %d\n", total);
    
    flushLoraQueue();
    
    return total;
}