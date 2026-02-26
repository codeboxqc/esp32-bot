/*
 * event_tracker.cpp - Event Tracking Implementation
 */

#include "event_tracker.h"
#include <EEPROM.h>
#include <string.h>
#include <TFT_eSPI.h>

// ==================== EEPROM Structure ====================

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint16_t count;
    uint16_t next_index;
    char event_ids[MAX_TRACKED_EVENTS][MAX_EVENT_ID_LEN];
} eeprom_data_t;

// ==================== Static Variables ====================

static char seen_event_ids[MAX_TRACKED_EVENTS][MAX_EVENT_ID_LEN];
static int seen_event_count = 0;
static int seen_event_index = 0;

// Display queue
static disaster_event_t display_queue[MAX_DISPLAY_QUEUE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

// ==================== EEPROM Protection ====================
static bool eeprom_valid = false;
static unsigned long last_eeprom_save = 0;
static const unsigned long EEPROM_SAVE_DELAY = 5000; // 5 seconds between saves

// ==================== EEPROM Functions ====================

static bool validate_eeprom_data(int count, int index) {
    // Validate count range
    if (count < 0 || count > MAX_TRACKED_EVENTS) {
        Serial.println("[EEPROM] Invalid count detected");
        return false;
    }
    
    // Validate index range
    if (index < 0 || index >= MAX_TRACKED_EVENTS) {
        Serial.println("[EEPROM] Invalid index detected");
        return false;
    }
    
    return true;
}

static void safe_eeprom_read(int addr, uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (addr + i < EEPROM_SIZE) {
            data[i] = EEPROM.read(addr + i);
        } else {
            data[i] = 0; // Read zero if beyond EEPROM size
            Serial.println("[EEPROM] Warning: Read beyond EEPROM size");
        }
    }
}

static void safe_eeprom_write(int addr, uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (addr + i < EEPROM_SIZE) {
            EEPROM.write(addr + i, data[i]);
        } else {
            Serial.println("[EEPROM] Warning: Write beyond EEPROM size");
            break;
        }
    }
}

static void load_from_eeprom(void) {
    EEPROM.begin(EEPROM_SIZE);
    eeprom_valid = false;
    
    uint8_t magic, version;
    safe_eeprom_read(0, &magic, 1);
    safe_eeprom_read(1, &version, 1);
    
    if (magic != EEPROM_MAGIC || version != EEPROM_VERSION) {
        Serial.println("[TRACKER] No valid EEPROM data, starting fresh");
        seen_event_count = 0;
        seen_event_index = 0;
        eeprom_valid = true;
        return;
    }
    
    uint8_t count_low, count_high;
    uint8_t index_low, index_high;
    
    safe_eeprom_read(2, &count_low, 1);
    safe_eeprom_read(3, &count_high, 1);
    safe_eeprom_read(4, &index_low, 1);
    safe_eeprom_read(5, &index_high, 1);
    
    int loaded_count = count_low | (count_high << 8);
    int loaded_index = index_low | (index_high << 8);
    
    if (!validate_eeprom_data(loaded_count, loaded_index)) {
        Serial.println("[EEPROM] Data validation failed, starting fresh");
        seen_event_count = 0;
        seen_event_index = 0;
        eeprom_valid = true;
        return;
    }
    
    seen_event_count = loaded_count;
    seen_event_index = loaded_index;
    
    // Load event IDs with bounds checking
    int addr = 6;
    int max_bytes = EEPROM_SIZE - 6;
    int bytes_to_read = seen_event_count * MAX_EVENT_ID_LEN;
    
    if (bytes_to_read > max_bytes) {
        Serial.println("[EEPROM] Data size exceeds EEPROM, truncating");
        bytes_to_read = max_bytes;
        seen_event_count = max_bytes / MAX_EVENT_ID_LEN;
    }
    
    for (int i = 0; i < seen_event_count && i < MAX_TRACKED_EVENTS; i++) {
        for (int j = 0; j < MAX_EVENT_ID_LEN; j++) {
            if (addr < EEPROM_SIZE) {
                seen_event_ids[i][j] = EEPROM.read(addr++);
            } else {
                seen_event_ids[i][j] = '\0';
            }
        }
    }
    
    eeprom_valid = true;
    Serial.printf("[TRACKER] Loaded %d seen events from EEPROM\n", seen_event_count);
}

void save_seen_events(void) {
    // Rate limit saves to prevent EEPROM wear
    if (millis() - last_eeprom_save < EEPROM_SAVE_DELAY) {
        return;
    }
    
    if (!eeprom_valid) {
        Serial.println("[EEPROM] Cannot save - EEPROM invalid");
        return;
    }
    
    EEPROM.begin(EEPROM_SIZE);
    
    uint8_t magic = EEPROM_MAGIC;
    uint8_t version = EEPROM_VERSION;
    uint8_t count_low = seen_event_count & 0xFF;
    uint8_t count_high = (seen_event_count >> 8) & 0xFF;
    uint8_t index_low = seen_event_index & 0xFF;
    uint8_t index_high = (seen_event_index >> 8) & 0xFF;
    
    safe_eeprom_write(0, &magic, 1);
    safe_eeprom_write(1, &version, 1);
    safe_eeprom_write(2, &count_low, 1);
    safe_eeprom_write(3, &count_high, 1);
    safe_eeprom_write(4, &index_low, 1);
    safe_eeprom_write(5, &index_high, 1);
    
    int addr = 6;
    int max_bytes = EEPROM_SIZE - 6;
    int bytes_to_write = seen_event_count * MAX_EVENT_ID_LEN;
    
    if (bytes_to_write > max_bytes) {
        Serial.println("[EEPROM] Too many events to save, truncating");
        bytes_to_write = max_bytes;
        seen_event_count = max_bytes / MAX_EVENT_ID_LEN;
    }
    
    for (int i = 0; i < seen_event_count && i < MAX_TRACKED_EVENTS; i++) {
        for (int j = 0; j < MAX_EVENT_ID_LEN && addr < EEPROM_SIZE; j++) {
            EEPROM.write(addr++, seen_event_ids[i][j]);
        }
    }
    
    if (EEPROM.commit()) {
        last_eeprom_save = millis();
        Serial.printf("[TRACKER] Saved %d events to EEPROM\n", seen_event_count);
    } else {
        Serial.println("[EEPROM] Commit failed!");
    }
}

// ==================== Public Functions ====================

void event_tracker_init(void) {
    memset(seen_event_ids, 0, sizeof(seen_event_ids));
    memset(display_queue, 0, sizeof(display_queue));
    
    seen_event_count = 0;
    seen_event_index = 0;
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    
    load_from_eeprom();
}

bool is_event_seen(const char *event_id) {
    if (!event_id || event_id[0] == '\0') return true;
    
    for (int i = 0; i < seen_event_count; i++) {
        if (strncmp(seen_event_ids[i], event_id, MAX_EVENT_ID_LEN - 1) == 0) {
            return true;
        }
    }
    return false;
}

void mark_event_seen(const char *event_id) {
    if (!event_id || event_id[0] == '\0') return;
    if (!eeprom_valid) return;
    if (is_event_seen(event_id)) return;
    
    // Add to circular buffer
    strncpy(seen_event_ids[seen_event_index], event_id, MAX_EVENT_ID_LEN - 1);
    seen_event_ids[seen_event_index][MAX_EVENT_ID_LEN - 1] = '\0';
    
    seen_event_index = (seen_event_index + 1) % MAX_TRACKED_EVENTS;
    
    if (seen_event_count < MAX_TRACKED_EVENTS) {
        seen_event_count++;
    }
    
    Serial.printf("[TRACKER] Marked seen: %s (total: %d)\n", event_id, seen_event_count);
    
    // Save every 5 events with rate limiting
    static int save_counter = 0;
    if (++save_counter >= 5) {
        save_seen_events();
        save_counter = 0;
    }
}

bool add_event_to_queue(disaster_event_t *event) {
    if (!event || event->id[0] == '\0') return false;
    
    if (is_event_seen(event->id)) {
        return false;
    }
    
    if (queue_count >= MAX_DISPLAY_QUEUE) {
        Serial.println("[TRACKER] Queue full, dropping oldest");
        queue_head = (queue_head + 1) % MAX_DISPLAY_QUEUE;
        queue_count--;
    }
    
    event->is_new = true;
    memcpy(&display_queue[queue_tail], event, sizeof(disaster_event_t));
    queue_tail = (queue_tail + 1) % MAX_DISPLAY_QUEUE;
    queue_count++;
    
    Serial.printf("[TRACKER] Queued: %s - %s (queue: %d)\n",
                  event->type, event->title, queue_count);
    
    return true;
}

bool get_next_event_from_queue(disaster_event_t *event) {
    if (queue_count == 0 || !event) return false;
    
    memcpy(event, &display_queue[queue_head], sizeof(disaster_event_t));
    queue_head = (queue_head + 1) % MAX_DISPLAY_QUEUE;
    queue_count--;
    
    mark_event_seen(event->id);
    
    return true;
}

int get_display_queue_count(void) {
    return queue_count;
}

void clear_display_queue(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

int get_seen_event_count(void) {
    return seen_event_count;
}

void clear_all_seen_events(void) {
    memset(seen_event_ids, 0, sizeof(seen_event_ids));
    seen_event_count = 0;
    seen_event_index = 0;
    eeprom_valid = true; // Reset validity
    save_seen_events();
    Serial.println("[TRACKER] Cleared all seen events");
}

const char* get_event_type_name(const char *code) {
    if (!code) return "ALERT";
    
    if (strcmp(code, "EQ") == 0) return "QUAKE";
    if (strcmp(code, "TC") == 0) return "CYCLONE";
    if (strcmp(code, "VO") == 0) return "VOLCANO";
    if (strcmp(code, "FL") == 0) return "FLOOD";
    if (strcmp(code, "DR") == 0) return "DROUGHT";
    if (strcmp(code, "WF") == 0) return "FIRE";
    if (strcmp(code, "LS") == 0) return "SLIDE";
    if (strcmp(code, "CW") == 0) return "C.WAVE";
    if (strcmp(code, "EP") == 0) return "EPIDEMIC";
    
    return "ALERT";
}

uint16_t get_alert_color(uint8_t alert_level) {
    switch (alert_level) {
        case 2:  return TFT_RED;
        case 1:  return TFT_ORANGE;
        default: return TFT_GREEN;
    }
}