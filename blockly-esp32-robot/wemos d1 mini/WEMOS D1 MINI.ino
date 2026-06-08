/*
 * WEMOS D1 MINI - Serial to WiFi UDP bridge
 */

#include <WiFi.h>
#include <WiFiUdp.h>

// CONFIGURATION
const char*    AP_SSID      = "RobotAP";
const char*    AP_PASS      = "robot1234";
// Convert the target address to an IPAddress object.
IPAddress      ESP32_IP(192, 168, 4, 1); 
const uint16_t UDP_SEND_PORT = 4210;
const uint16_t UDP_RECV_PORT = 4211;
const uint32_t SERIAL_BAUD   = 115200;
const uint16_t MAX_PKT       = 256;

// STATE
WiFiUDP udp; // WiFiUDP must use the exact class capitalization.
char    serialBuf[MAX_PKT];
int     serialIdx = 0;
char    udpBuf[MAX_PKT];
bool    wifiOk = false;
unsigned long lastReconnect = 0;

// HELPERS
/*
 * Connects the WEMOS board to the ESP32 access point.
 *
 * The ESP32 sketch creates the WiFi network, so this board runs in station
 * mode and joins that network using AP_SSID and AP_PASS. The function waits
 * for a limited number of connection attempts, starts the UDP listener after
 * a successful connection, and reports the result over Serial so the desktop
 * app can show connection status.
 */
void connectWiFi() {
  Serial.println("WEMOS:WIFI_CONNECTING:" + String(AP_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(250); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    udp.begin(UDP_RECV_PORT); // UDP is declared correctly before use.
    Serial.println("WEMOS:WIFI_OK:" + WiFi.localIP().toString());
    Serial.println("WEMOS:READY");
  } else {
    wifiOk = false;
    Serial.println("WEMOS:WIFI_FAIL");
  }
}

// SETUP
/*
 * Initializes the WEMOS bridge after reset or power-up.
 *
 * Serial is the link to the computer/Electron app, while WiFi/UDP is the link
 * to the ESP32 robot controller. setup() starts Serial first so boot and WiFi
 * status messages are visible immediately, then calls connectWiFi() to prepare
 * the UDP bridge before loop() starts forwarding traffic.
 */
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("WEMOS:BOOTING");
  connectWiFi();
}

// LOOP
/*
 * Runs the bidirectional bridge between Serial and UDP.
 *
 * The loop has three responsibilities:
 * 1. Collect one complete Serial command line from the computer and send it to
 *    the ESP32 over UDP.
 * 2. Check for UDP telemetry replies from the ESP32 and print them to Serial.
 * 3. Detect lost WiFi connections and periodically try to reconnect.
 *
 * Commands are line-based. A newline or carriage return marks the end of one
 * command, and MAX_PKT prevents the Serial buffer from overflowing.
 */
void loop() {
  // 1. Read serial input and send it to the ESP32 over UDP.
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialIdx > 0) {
        // Terminate the buffered line, trim whitespace, then reuse the buffer.
        serialBuf[serialIdx] = '\0';
        String cmd = String(serialBuf); cmd.trim();
        serialIdx = 0;
        if (cmd.length() == 0) continue;
        if (wifiOk) {
          // UDP packets are addressed directly to the ESP32 access point IP.
          udp.beginPacket(ESP32_IP, UDP_SEND_PORT);
          udp.write((uint8_t*)cmd.c_str(), cmd.length());
          udp.endPacket();
          Serial.println("WEMOS:SENT:" + cmd);
        } else {
          Serial.println("WEMOS:NO_WIFI");
        }
      }
    } else if (serialIdx < MAX_PKT - 1) {
      // Store characters until a line ending arrives, leaving room for '\0'.
      serialBuf[serialIdx++] = c;
    }
  }

  // 2. Read UDP packets from the ESP32 and forward them over serial.
  if (wifiOk) {
    int pktSize = udp.parsePacket();
    if (pktSize > 0) {
      int len = udp.read(udpBuf, MAX_PKT - 1);
      if (len > 0) { 
        // Null-terminate the UDP payload so it can be safely wrapped as String.
        udpBuf[len] = '\0'; 
        Serial.println("ESP32:" + String(udpBuf)); 
      }
    }
  }

  // 3. Automatic reconnection.
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiOk) { wifiOk = false; Serial.println("WEMOS:WIFI_LOST"); }
    unsigned long now = millis();
    if (now - lastReconnect > 3000) {
      // Retry at a fixed interval instead of blocking loop() continuously.
      lastReconnect = now;
      connectWiFi();
    }
  }
}
