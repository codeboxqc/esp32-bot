#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "event_tracker.h"

extern TFT_eSPI tft; // Expose TFT object from main.cpp

void display_startup_screen(void);
void display_status(const char *line1, const char *line2);
void display_error(const char *line1, const char *line2);
void display_wifi_connecting(int progress);
void display_next_alert(void);
void display_idle_screen(void);
void display_update(void); // Keeping for compatibility, but TFT doesn't require refresh
void display_mesh_chat(const char* message); 


#endif // DISPLAY_RENDERER_H