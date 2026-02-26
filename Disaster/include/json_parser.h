/*
 * json_parser.h - HTTP Fetcher and JSON Parser
 * 
 * Fetches data from disaster APIs and parses JSON responses
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <Arduino.h>
#include "config.h"
#include "event_tracker.h"

/**
 * Initialize HTTP client
 */
void http_init(void);

/**
 * Fetch and parse USGS earthquake data
 * Returns number of new events found
 */
int fetch_usgs_data(void);

/**
 * Fetch and parse GDACS disaster data
 * Returns number of new events found
 */
int fetch_gdacs_data(void);

/**
 * Fetch all API sources
 * Returns total number of new events found
 */
int fetch_all_data(void);

/**
 * Check if WiFi is connected
 */
bool is_wifi_connected(void);

/**
 * Get WiFi status string
 */
const char* get_wifi_status(void);

#endif // JSON_PARSER_H
