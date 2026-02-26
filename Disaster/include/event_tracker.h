#ifndef EVENT_TRACKER_H
#define EVENT_TRACKER_H

#include <Arduino.h>
#include "config.h"

typedef struct {
    char id[MAX_EVENT_ID_LEN];      
    char type[MAX_TYPE_LEN];        
    char title[MAX_TITLE_LEN];      
    char country[MAX_COUNTRY_LEN];  
    float magnitude;                 
    float latitude;                  
    float longitude;                 
    uint8_t alert_level;            
    bool is_new;                     
} disaster_event_t;

void event_tracker_init(void);
bool is_event_seen(const char *event_id);
void mark_event_seen(const char *event_id);
bool add_event_to_queue(disaster_event_t *event);
bool get_next_event_from_queue(disaster_event_t *event);
int get_display_queue_count(void);
void clear_display_queue(void);
int get_seen_event_count(void);
void clear_all_seen_events(void);
void save_seen_events(void);
const char* get_event_type_name(const char *code);
uint16_t get_alert_color(uint8_t alert_level); // Changed return type

#endif // EVENT_TRACKER_H