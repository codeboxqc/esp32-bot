#include "display_renderer.h"
#include <string.h>

void display_startup_screen(void) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("DISASTER", 120, 50, 4); 
    tft.drawString("ALERT", 120, 85, 4);
}

void display_status(const char *line1, const char *line2) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    if (line1) tft.drawString(line1, 120, 50, 4);
    if (line2) tft.drawString(line2, 120, 85, 4);
}

void display_error(const char *line1, const char *line2) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 240, 135, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    if (line1) tft.drawString(line1, 120, 45, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (line2) tft.drawString(line2, 120, 85, 2);
}

void display_wifi_connecting(int progress) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Connecting", 120, 50, 4);
    
    String dots = "WiFi ";
    int numDots = (progress / 10) % 6;
    for (int i = 0; i < numDots; i++) dots += ".";
    tft.drawString(dots, 120, 85, 4);
}

void display_next_alert(void) {
    disaster_event_t event;
    if (!get_next_event_from_queue(&event)) return;
    
    Serial.printf("[DISPLAY] Showing: %s - %s\n", event.type, event.title);
    tft.fillScreen(TFT_BLACK);
    
    uint16_t alert_color = get_alert_color(event.alert_level);
    tft.fillRect(0, 0, 240, 10, alert_color);
    
    // Header
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(alert_color, TFT_BLACK);
    tft.drawString(get_event_type_name(event.type), 10, 20, 4);
    
    // Magnitude
    if (event.magnitude > 0) {
        char mag_str[16];
        snprintf(mag_str, sizeof(mag_str), "M%.1f", event.magnitude);
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(mag_str, 230, 20, 4);
    }
    
    // Title / Location (Wrapped)
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 60);
    tft.setTextFont(2);
    tft.setTextSize(2); // Scale to 32px height
    tft.setTextWrap(true, true);
    tft.print(event.title);
    
    // NEW indicator
    if (event.is_new) {
        tft.setTextDatum(BR_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("NEW", 230, 125, 2);
    }
}

void display_idle_screen(void) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 240, 135, TFT_GREEN);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("MONITORING", 120, 50, 4);
    
    tft.setTextColor(TFT_DARKCYAN, TFT_BLACK);
    tft.drawString("NO NEW ALERTS", 120, 85, 4);
    
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "Events: %d", get_seen_event_count());
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(count_str, 120, 125, 2);
}

void display_update(void) {
    // Left intentionally blank. TFT_eSPI doesn't need manual continuous refreshing
}


void display_mesh_chat(const char* message) {
    // 1. Clear the screen
    tft.fillScreen(TFT_BLACK);
    
    // 2. Draw a blue border to indicate it's a chat message, not an alert
    tft.drawRect(0, 0, 240, 135, TFT_BLUE);
    tft.drawRect(1, 1, 238, 133, TFT_BLUE); 
    
    // 3. Draw Header
    tft.setTextDatum(TC_DATUM);             // Top-Center alignment
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
    tft.drawString("MESH CHAT", 120, 10, 4); // Use Font 4 for big header
    
    // 4. Draw a dividing line under the header
    tft.drawLine(10, 35, 230, 35, TFT_BLUE);
    
    // 5. Print the actual message text
    tft.setTextDatum(TL_DATUM);              // Top-Left alignment
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Yellow text
    tft.setCursor(10, 45);                   // Start drawing below the line
    tft.setTextFont(2);                      // Standard font
    tft.setTextSize(2);                      // Make it slightly larger
    tft.setTextWrap(true, true);             // Wrap text if it hits the right edge
    
    tft.print(message);                      // Print the chat string!
}