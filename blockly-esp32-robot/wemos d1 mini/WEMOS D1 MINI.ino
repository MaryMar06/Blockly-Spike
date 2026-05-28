/*
 * WEMOS D1 MINI — Serial↔WiFi UDP Bridge
 */

#include <WiFi.h>
#include <WiFiUdp.h>

// ── CONFIGURACIÓN ────────────────────────────────────────────
const char*    AP_SSID      = "RobotAP";
const char*    AP_PASS      = "robot1234";
// Convertimos el string a objeto IPAddress correctamente
IPAddress      ESP32_IP(192, 168, 4, 1); 

const uint16_t UDP_SEND_PORT = 4210;
const uint16_t UDP_RECV_PORT = 4211;
const uint32_t SERIAL_BAUD   = 115200;
const uint16_t MAX_PKT       = 256;

// ── ESTADO ───────────────────────────────────────────────────
WiFiUDP udp; // <-- Corregido a WiFiUDP (todo en mayúsculas)
char    serialBuf[MAX_PKT];
int     serialIdx = 0;
char    udpBuf[MAX_PKT];
bool    wifiOk = false;
unsigned long lastReconnect = 0;

// ── HELPERS ──────────────────────────────────────────────────
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
    udp.begin(UDP_RECV_PORT); // Ahora udp ya está declarada correctamente
    Serial.println("WEMOS:WIFI_OK:" + WiFi.localIP().toString());
    Serial.println("WEMOS:READY");
  } else {
    wifiOk = false;
    Serial.println("WEMOS:WIFI_FAIL");
  }
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("WEMOS:BOOTING");
  connectWiFi();
}

// ── LOOP ─────────────────────────────────────────────────────
void loop() {
  // 1. Leer Serial → enviar UDP al ESP32
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialIdx > 0) {
        serialBuf[serialIdx] = '\0';
        String cmd = String(serialBuf); cmd.trim();
        serialIdx = 0;
        if (cmd.length() == 0) continue;
        if (wifiOk) {
          udp.beginPacket(ESP32_IP, UDP_SEND_PORT);
          udp.write((uint8_t*)cmd.c_str(), cmd.length());
          udp.endPacket();
          Serial.println("WEMOS:SENT:" + cmd);
        } else {
          Serial.println("WEMOS:NO_WIFI");
        }
      }
    } else if (serialIdx < MAX_PKT - 1) {
      serialBuf[serialIdx++] = c;
    }
  }

  // 2. Leer UDP del ESP32 → reenviar por Serial
  if (wifiOk) {
    int pktSize = udp.parsePacket();
    if (pktSize > 0) {
      int len = udp.read(udpBuf, MAX_PKT - 1);
      if (len > 0) { 
        udpBuf[len] = '\0'; 
        Serial.println("ESP32:" + String(udpBuf)); 
      }
    }
  }

  // 3. Reconexión automática
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiOk) { wifiOk = false; Serial.println("WEMOS:WIFI_LOST"); }
    unsigned long now = millis();
    if (now - lastReconnect > 3000) {
      lastReconnect = now;
      connectWiFi();
    }
  }
}