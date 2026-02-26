

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



<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Disaster Alert System - Documentation</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;
            line-height: 1.6;
            color: #24292e;
            max-width: 1100px;
            margin: 0 auto;
            padding: 20px;
            background-color: #ffffff;
        }
        
        h1, h2, h3, h4 {
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
        }
        
        h1 {
            padding-bottom: 0.3em;
            font-size: 2em;
            border-bottom: 1px solid #eaecef;
        }
        
        h2 {
            padding-bottom: 0.3em;
            font-size: 1.5em;
            border-bottom: 1px solid #eaecef;
        }
        
        h3 {
            font-size: 1.25em;
        }
        
        code {
            padding: 0.2em 0.4em;
            margin: 0;
            font-size: 85%;
            background-color: #f6f8fa;
            border-radius: 3px;
            font-family: 'SF Mono', Monaco, Consolas, 'Liberation Mono', Courier, monospace;
        }
        
        pre {
            padding: 16px;
            overflow: auto;
            font-size: 85%;
            line-height: 1.45;
            background-color: #f6f8fa;
            border-radius: 3px;
        }
        
        pre code {
            padding: 0;
            margin: 0;
            background-color: transparent;
        }
        
        table {
            border-spacing: 0;
            border-collapse: collapse;
            width: 100%;
            margin-bottom: 16px;
        }
        
        table th, table td {
            padding: 6px 13px;
            border: 1px solid #dfe2e5;
        }
        
        table th {
            font-weight: 600;
            background-color: #f6f8fa;
        }
        
        blockquote {
            padding: 0 1em;
            color: #6a737d;
            border-left: 0.25em solid #dfe2e5;
        }
        
        img {
            max-width: 100%;
            box-sizing: content-box;
        }
        
        hr {
            height: 0.25em;
            padding: 0;
            margin: 24px 0;
            background-color: #e1e4e8;
            border: 0;
        }
        
        .markdown-body {
            box-sizing: border-box;
            min-width: 200px;
            max-width: 980px;
            margin: 0 auto;
        }
        
        @media (max-width: 767px) {
            body {
                padding: 15px;
            }
        }
        
        .badge {
            display: inline-block;
            padding: 4px 8px;
            font-size: 12px;
            font-weight: 600;
            line-height: 1;
            color: #fff;
            text-align: center;
            white-space: nowrap;
            vertical-align: baseline;
            border-radius: 3px;
            background-color: #586069;
        }
        
        .badge.primary {
            background-color: #0366d6;
        }
        
        .badge.success {
            background-color: #28a745;
        }
        
        .badge.warning {
            background-color: #f66a0a;
        }
        
        .badge.danger {
            background-color: #d73a49;
        }
        
        .mermaid-diagram {
            background-color: #f9f9f9;
            padding: 20px;
            border-radius: 5px;
            text-align: center;
            border: 1px solid #e1e4e8;
            margin: 20px 0;
        }
        
        .btn-download {
            display: inline-block;
            padding: 10px 20px;
            background-color: #2c3e50;
            color: white;
            text-decoration: none;
            border-radius: 5px;
            font-weight: bold;
            margin: 20px 0;
        }
        
        .btn-download:hover {
            background-color: #34495e;
        }
        
        .container {
            border: 1px solid #e1e4e8;
            padding: 20px;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <div class="markdown-body">
        <h1>Disaster Alert System - Technical Documentation</h1>
        
        <p>
            <span class="badge primary">Version 2.2</span>
            <span class="badge success">ESP32 TTGO T-Display</span>
            <span class="badge warning">LoRa Mesh</span>
            <span class="badge danger">Real-time Alerts</span>
        </p>
        
        <hr>
        
        <p>
            <a href="#" class="btn-download" onclick="downloadMarkdown()">📥 Download as .md</a>
            <a href="#" class="btn-download" style="background-color: #27ae60;" onclick="downloadHTML()">📥 Download as HTML</a>
        </p>

        <h2>📋 Project Overview</h2>
        
        <p>A real-time disaster monitoring and alert system built for the <strong>ESP32 TTGO T-Display</strong>, featuring WiFi-based data fetching from USGS and GDACS APIs, LoRa mesh networking integration, and a comprehensive display interface. The system automatically fetches earthquake and disaster data, displays alerts on the built-in TFT screen, and broadcasts critical information over a Meshtastic LoRa mesh network.</p>
        
        <div style="background-color: #f0f7ff; padding: 20px; border-radius: 5px; border-left: 4px solid #0366d6; margin: 20px 0;">
            <strong>🎯 Purpose:</strong> Provide real-time disaster alerts in areas with limited internet connectivity through mesh network propagation.
        </div>

        <h2>🚀 Features</h2>
        
        <ul>
            <li><strong>Real-time Disaster Monitoring</strong>
                <ul>
                    <li>USGS Earthquake feed (4.5+ magnitude events)</li>
                    <li>GDACS global disaster alerts (cyclones, floods, volcanoes, etc.)</li>
                    <li>Automatic 5-minute refresh interval</li>
                </ul>
            </li>
            <li><strong>Smart Alert Management</strong>
                <ul>
                    <li>Event deduplication using EEPROM storage</li>
                    <li>Priority queuing system (up to 5 events)</li>
                    <li>Color-coded alert levels (Green/Orange/Red)</li>
                    <li>"NEW" indicator for unviewed events</li>
                </ul>
            </li>
            <li><strong>Display Capabilities</strong>
                <ul>
                    <li>240x135 TFT color display</li>
                    <li>Multiple screen states (startup, connecting, fetching, alerts)</li>
                    <li>Text wrapping for long location descriptions</li>
                    <li>Visual alert severity indicators</li>
                </ul>
            </li>
            <li><strong>LoRa Mesh Integration</strong>
                <ul>
                    <li>Bidirectional communication with Meshtastic nodes</li>
                    <li>Automatic alert broadcasting to mesh network</li>
                    <li>Incoming chat message display</li>
                    <li>Message queueing with retry logic</li>
                </ul>
            </li>
            <li><strong>User Interface</strong>
                <ul>
                    <li>Two physical buttons for control</li>
                    <li>Serial/UART command interface</li>
                    <li>Button hold detection (3 seconds) for emergency reset</li>
                    <li>EEPROM clear functionality</li>
                </ul>
            </li>
        </ul>

        <h2>🛠️ Hardware Requirements</h2>
        
        <table>
            <thead>
                <tr>
                    <th>Component</th>
                    <th>Specification</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td><strong>Microcontroller</strong></td>
                    <td>ESP32 TTGO T-Display</td>
                </tr>
                <tr>
                    <td><strong>Display</strong></td>
                    <td>1.14" ST7789V TFT (240x135)</td>
                </tr>
                <tr>
                    <td><strong>LoRa Module</strong></td>
                    <td>Meshtastic-compatible (Heltec, etc.)</td>
                </tr>
                <tr>
                    <td><strong>Connectivity</strong></td>
                    <td>WiFi 2.4GHz, UART @ 9600 baud</td>
                </tr>
                <tr>
                    <td><strong>GPIO Used</strong></td>
                    <td>Button 1 (35), Button 2 (0), Backlight (4), Mesh TX (27), Mesh RX (25)</td>
                </tr>
            </tbody>
        </table>

        <h2>📦 Software Dependencies</h2>
        
        <pre><code>- Arduino Core for ESP32
- TFT_eSPI Library
- ArduinoJson (v7)
- WiFiClientSecure
- HTTPClient
- EEPROM Library</code></pre>

        <h2>📁 Project Structure</h2>
        
        <pre><code>disaster-alert-system/
├── main.cpp                 # Main application loop and setup
├── display_renderer.cpp     # TFT display rendering functions
├── event_tracker.cpp        # Event queue and EEPROM management
├── json_parser.cpp          # HTTP fetching and JSON parsing
├── display_renderer.h       # Display function prototypes
├── event_tracker.h          # Event tracking prototypes and structs
└── json_parser.h            # Parser prototypes and configuration</code></pre>

        <h2>🔧 Core Components</h2>
        
        <h3>1. Main Application (<code>main.cpp</code>)</h3>
        <p>The central control unit handling:</p>
        <ul>
            <li>System initialization and WiFi connection</li>
            <li>Button input processing</li>
            <li>Periodic API fetching</li>
            <li>Display state management</li>
            <li>LoRa mesh communication</li>
        </ul>

        <h3>2. Display Renderer (<code>display_renderer.cpp</code>)</h3>
        <p>Manages all screen rendering:</p>
        <ul>
            <li>Startup splash screen</li>
            <li>Status messages</li>
            <li>Alert displays with color coding</li>
            <li>Mesh chat message display</li>
            <li>Idle monitoring screen</li>
        </ul>

        <h3>3. Event Tracker (<code>event_tracker.cpp</code>)</h3>
        <p>Handles event lifecycle:</p>
        <ul>
            <li>EEPROM-based event storage (512 bytes)</li>
            <li>Event deduplication (up to 20 tracked events)</li>
            <li>Display queue management (5 events)</li>
            <li>Alert level color mapping</li>
            <li>Event type name conversion</li>
        </ul>

        <h3>4. JSON Parser (<code>json_parser.cpp</code>)</h3>
        <p>Manages external API communication:</p>
        <ul>
            <li>USGS earthquake feed parsing</li>
            <li>GDACS disaster alert parsing</li>
            <li>HTTP rate limiting (2 seconds between requests)</li>
            <li>Memory-safe JSON processing</li>
            <li>LoRa message sanitization (UTF-8 to ASCII)</li>
        </ul>

        <h2>💾 Data Structures</h2>
        
        <h3>Disaster Event</h3>
        <pre><code>typedef struct {
    char id[24];           // Unique event identifier
    char type[8];          // Event type (EQ, TC, VO, etc.)
    char title[64];        // Event title/description
    char country[32];      // Affected country
    float magnitude;       // Event magnitude/severity
    uint8_t alert_level;   // 0=Green, 1=Orange, 2=Red
    bool is_new;           // New event flag
} disaster_event_t;</code></pre>

        <h3>EEPROM Storage Layout</h3>
        <table>
            <thead>
                <tr>
                    <th>Address</th>
                    <th>Data</th>
                    <th>Size</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td>0</td>
                    <td>Magic Byte (0xDA)</td>
                    <td>1 byte</td>
                </tr>
                <tr>
                    <td>1</td>
                    <td>Version (0x01)</td>
                    <td>1 byte</td>
                </tr>
                <tr>
                    <td>2-3</td>
                    <td>Event Count</td>
                    <td>2 bytes</td>
                </tr>
                <tr>
                    <td>4-5</td>
                    <td>Next Index</td>
                    <td>2 bytes</td>
                </tr>
                <tr>
                    <td>6+</td>
                    <td>Event IDs</td>
                    <td>20 × 24 bytes</td>
                </tr>
            </tbody>
        </table>

        <h2>🔄 System Workflow</h2>
        
        <div class="mermaid-diagram">
            <pre>
System Start → Initialize Display → Connect WiFi → Fetch API Data → 
New Events? → Yes → Add to Queue → Display Alerts → Broadcast via LoRa → Wait 5 Minutes
         ↓ No
    Show Idle Screen → Wait 5 Minutes
            </pre>
            <p><em>Flow: System continuously monitors and displays alerts every 5 minutes</em></p>
        </div>

        <h2>🎨 Display States</h2>
        
        <table>
            <thead>
                <tr>
                    <th>State</th>
                    <th>Description</th>
                    <th>Colors</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td><strong>Startup</strong></td>
                    <td>"DISASTER ALERT"</td>
                    <td>Cyan on Black</td>
                </tr>
                <tr>
                    <td><strong>Connecting</strong></td>
                    <td>Animated "CONNECTING..."</td>
                    <td>Yellow on Black</td>
                </tr>
                <tr>
                    <td><strong>Connected</strong></td>
                    <td>IP Address display</td>
                    <td>Green on Black</td>
                </tr>
                <tr>
                    <td><strong>Fetching</strong></td>
                    <td>"FETCHING DATA..."</td>
                    <td>Orange on Black</td>
                </tr>
                <tr>
                    <td><strong>Alert</strong></td>
                    <td>Event details with magnitude</td>
                    <td>Red/Orange/Green header</td>
                </tr>
                <tr>
                    <td><strong>Idle</strong></td>
                    <td>"MONITORING - NO ALERTS"</td>
                    <td>Green border</td>
                </tr>
                <tr>
                    <td><strong>Error</strong></td>
                    <td>Error message with red border</td>
                    <td>Red/White</td>
                </tr>
                <tr>
                    <td><strong>Chat</strong></td>
                    <td>Incoming mesh message</td>
                    <td>Blue border</td>
                </tr>
            </tbody>
        </table>

        <h2>📡 API Endpoints</h2>
        
        <h3>USGS Earthquake Feed</h3>
        <pre><code>URL: https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson
Format: GeoJSON
Rate: Limited to 1 request per 2 seconds</code></pre>

        <h3>GDACS Disaster Feed</h3>
        <pre><code>URL: https://gdacs.org/xml/rss_24h.xml
Format: GeoJSON
Features: Cyclones, floods, volcanoes, droughts, fires</code></pre>

        <h2>🔌 Pin Configuration</h2>
        
        <table>
            <thead>
                <tr>
                    <th>Function</th>
                    <th>GPIO Pin</th>
                    <th>Notes</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td>TFT Backlight</td>
                    <td>4</td>
                    <td>Active HIGH</td>
                </tr>
                <tr>
                    <td>Button 1</td>
                    <td>35</td>
                    <td>External pull-up</td>
                </tr>
                <tr>
                    <td>Button 2</td>
                    <td>0</td>
                    <td>Internal pull-up</td>
                </tr>
                <tr>
                    <td>Mesh TX</td>
                    <td>27</td>
                    <td>To Heltec RX</td>
                </tr>
                <tr>
                    <td>Mesh RX</td>
                    <td>25</td>
                    <td>From Heltec TX</td>
                </tr>
                <tr>
                    <td>TFT SPI</td>
                    <td>5,18,23</td>
                    <td>Standard SPI</td>
                </tr>
            </tbody>
        </table>

        <h2>🎮 User Controls</h2>
        
        <h3>Physical Buttons</h3>
        <ul>
            <li><strong>Button 1 (GPIO35)</strong>
                <ul>
                    <li>Short press: Send test LoRa message</li>
                    <li>Long press (3s): Emergency clear + reboot</li>
                </ul>
            </li>
            <li><strong>Button 2 (GPIO0)</strong>
                <ul>
                    <li>Short press: Clear EEPROM + refetch</li>
                    <li>Long press (3s): Emergency clear + reboot</li>
                </ul>
            </li>
        </ul>

        <h3>Serial Commands</h3>
        <ul>
            <li><code>C</code> or <code>c</code>: Clear EEPROM and refetch</li>
            <li><code>T</code> or <code>t</code>: Send test LoRa message</li>
        </ul>

        <h2>📊 Performance Characteristics</h2>
        
        <ul>
            <li><strong>WiFi Connection Time</strong>: &lt; 30 seconds</li>
            <li><strong>API Fetch Time</strong>: 2-5 seconds</li>
            <li><strong>Display Duration</strong>: 8 seconds per alert</li>
            <li><strong>EEPROM Save Delay</strong>: 5 seconds minimum</li>
            <li><strong>Memory Usage</strong>: ~20KB free heap minimum required</li>
            <li><strong>Event Tracking</strong>: Up to 20 unique events</li>
        </ul>

        <h2>🚨 Alert Levels</h2>
        
        <table>
            <thead>
                <tr>
                    <th>Level</th>
                    <th>Color</th>
                    <th>Criteria</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td>2 (High)</td>
                    <td><span style="color: #d73a49;">Red</span></td>
                    <td>M7.0+ earthquake / Red GDACS alert</td>
                </tr>
                <tr>
                    <td>1 (Medium)</td>
                    <td><span style="color: #f66a0a;">Orange</span></td>
                    <td>M5.5-6.9 earthquake / Orange GDACS alert</td>
                </tr>
                <tr>
                    <td>0 (Low)</td>
                    <td><span style="color: #28a745;">Green</span></td>
                    <td>&lt;M5.5 earthquake / Green GDACS alert</td>
                </tr>
            </tbody>
        </table>

        <h2>🔧 Configuration Options</h2>
        
        <p>In <code>main.cpp</code>:</p>
        <pre><code>#define FETCH_INTERVAL_MS   (5UL * 60UL * 1000UL)  // 5 minutes
#define DISPLAY_DURATION_MS (8UL * 1000UL)         // 8 seconds
#define WIFI_TIMEOUT_MS     30000                   // 30 seconds
#define BUTTON_HOLD_TIME    3000                    // 3 seconds</code></pre>

        <h2>🛡️ Error Handling</h2>
        
        <ul>
            <li><strong>WiFi Loss</strong>: Shows error screen, continues monitoring</li>
            <li><strong>HTTP Timeout</strong>: Automatic retry on next interval</li>
            <li><strong>Low Memory</strong>: Skips JSON parsing to prevent crashes</li>
            <li><strong>EEPROM Corruption</strong>: Auto-resets to fresh state</li>
            <li><strong>Queue Overflow</strong>: Drops oldest event, logs warning</li>
        </ul>

        <h2>📡 Mesh Integration</h2>
        
        <p>The system integrates with Meshtastic networks by:</p>
        <ol>
            <li>Queueing alerts for broadcast (up to 5 messages)</li>
            <li>Flushing queue every 500ms</li>
            <li>Monitoring incoming chat messages</li>
            <li>Displaying chat messages for 5 seconds</li>
            <li>ASCII sanitization for mesh compatibility</li>
        </ol>

        <h2>🔄 Future Improvements</h2>
        
        <ul>
            <li>[ ] Add more disaster data sources (NOAA, etc.)</li>
            <li>[ ] Implement over-the-air (OTA) updates</li>
            <li>[ ] Add GPS integration for location-based filtering</li>
            <li>[ ] Battery level monitoring</li>
            <li>[ ] Configurable alert thresholds via web interface</li>
            <li>[ ] Data logging to SD card</li>
            <li>[ ] Multi-language support</li>
        </ul>

        <h2>📝 License</h2>
        
        <p>This project is open-source hardware and software. Feel free to modify and distribute for disaster preparedness applications.</p>

        <h2>👥 Contributing</h2>
        
        <p>Contributions welcome! Please:</p>
        <ol>
            <li>Fork the repository</li>
            <li>Create a feature branch</li>
            <li>Submit a pull request</li>
            <li>Include detailed documentation</li>
        </ol>

        <h2>📞 Support</h2>
        
        <p>For issues and questions:</p>
        <ul>
            <li>Check Serial monitor output</li>
            <li>Verify wiring connections</li>
            <li>Ensure API endpoints are accessible</li>
            <li>Monitor free heap memory</li>
        </ul>

        <hr>
        
        <p style="text-align: center;">
            <strong>Version</strong>: 2.2<br>
            <strong>Last Updated</strong>: December 2024<br>
            <strong>Author</strong>: ESP32 Disaster Alert Team
        </p>
    </div>

    <script>
        function downloadMarkdown() {
            // This would contain the markdown content from the previous response
            const markdownContent = `# Disaster Alert System - Technical Documentation

## 📋 Project Overview

A real-time disaster monitoring and alert system built for the **ESP32 TTGO T-Display**, featuring WiFi-based data fetching from USGS and GDACS APIs, LoRa mesh networking integration, and a comprehensive display interface. The system automatically fetches earthquake and disaster data, displays alerts on the built-in TFT screen, and broadcasts critical information over a Meshtastic LoRa mesh network.

...`; // Full markdown content here
            
            const blob = new Blob([markdownContent], { type: 'text/markdown' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'DISASTER_ALERT_SYSTEM.md';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
        }
        
        function downloadHTML() {
            // Get the current page HTML
            const htmlContent = document.documentElement.outerHTML;
            const blob = new Blob([htmlContent], { type: 'text/html' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'DISASTER_ALERT_SYSTEM.html';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
        }
    </script>
</body>
</html>

