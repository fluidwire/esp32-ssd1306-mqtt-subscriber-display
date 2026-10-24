/*
 * ESP32 MQTT Message Display on SSD1306 OLED
 * ------------------------------------------
 * Subscribes to one MQTT topic and shows every message it receives on a
 * 128x64 I2C SSD1306 OLED, word-wrapped to the screen width. A header line
 * shows WiFi/MQTT state and how long ago the message arrived. If no new
 * message arrives for MESSAGE_TIMEOUT_MS the board falls back to an idle
 * "waiting" screen, so stale text never looks current. A simple, cheap
 * notice board for a shop, lab bench or dorm room.
 *
 * Required libraries (install via Library Manager):
 *   - "Adafruit SSD1306" by Adafruit
 *   - "Adafruit GFX Library" by Adafruit (pulled in by SSD1306)
 *   - "PubSubClient" by Nick O'Leary
 *
 * Board: any ESP32 dev board (ESP32 core for Arduino installed via Boards Mgr)
 *
 * Wiring:
 *   SSD1306 VCC -> ESP32 3V3
 *   SSD1306 GND -> ESP32 GND
 *   SSD1306 SDA -> ESP32 GPIO21
 *   SSD1306 SCL -> ESP32 GPIO22
 *
 * Setup:
 *   1. Copy config.h.example to config.h
 *   2. Fill in WiFi + MQTT broker details and a unique topic name.
 *   3. Flash. Open Serial Monitor at 115200.
 *   4. mosquitto_pub -h <broker> -t "<your topic>" -m "Meeting at 3PM, room 2"
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C   // some modules use 0x3D

const uint32_t MESSAGE_TIMEOUT_MS = 60000;   // go idle after 1 minute of silence
const uint32_t REDRAW_INTERVAL_MS = 1000;    // refresh header / "age" once a second
const uint8_t  CHARS_PER_LINE     = 21;      // 6px font on a 128px-wide screen
const uint8_t  BODY_LINES         = 6;       // lines available under the header
const size_t   MAX_MESSAGE_LEN    = 160;
const uint16_t MQTT_BUFFER        = 256;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

char     message[MAX_MESSAGE_LEN + 1] = "";
bool     haveMessage   = false;
uint32_t messageAt     = 0;
uint32_t lastRedraw    = 0;
uint32_t lastReconnect = 0;

// Draw the message word-wrapped into at most BODY_LINES lines.
void drawWrapped(const char* text) {
  char line[CHARS_PER_LINE + 1];
  uint8_t lineLen = 0;
  uint8_t lines = 0;
  size_t i = 0;
  size_t n = strlen(text);

  display.setCursor(0, 12);
  while (i < n && lines < BODY_LINES) {
    // find the next word
    size_t j = i;
    while (j < n && text[j] != ' ' && text[j] != '\n') j++;
    size_t wordLen = j - i;

    // hard-break a word that is longer than a whole line
    if (wordLen > CHARS_PER_LINE) {
      wordLen = CHARS_PER_LINE;
      j = i + wordLen;
    }

    if (lineLen > 0 && lineLen + 1 + wordLen > CHARS_PER_LINE) {
      line[lineLen] = '\0';
      display.println(line);
      lines++;
      lineLen = 0;
      if (lines >= BODY_LINES) break;
    }
    if (lineLen > 0) line[lineLen++] = ' ';
    memcpy(line + lineLen, text + i, wordLen);
    lineLen += wordLen;

    bool newline = (j < n && text[j] == '\n');
    i = j;
    while (i < n && (text[i] == ' ' || text[i] == '\n')) i++;

    if (newline) {
      line[lineLen] = '\0';
      display.println(line);
      lines++;
      lineLen = 0;
    }
  }
  if (lineLen > 0 && lines < BODY_LINES) {
    line[lineLen] = '\0';
    display.println(line);
  }
}

void redraw() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header: link state on the left, message age on the right
  display.setCursor(0, 0);
  display.print(WiFi.status() == WL_CONNECTED ? F("WiFi") : F("no WiFi"));
  display.print(mqtt.connected() ? F(" MQTT") : F(" --"));
  if (haveMessage) {
    uint32_t age = (millis() - messageAt) / 1000;
    char buf[12];
    snprintf(buf, sizeof(buf), "%lus ago", (unsigned long)age);
    display.setCursor(SCREEN_WIDTH - 6 * strlen(buf), 0);
    display.print(buf);
  }
  display.drawLine(0, 9, SCREEN_WIDTH - 1, 9, SSD1306_WHITE);

  if (haveMessage) {
    drawWrapped(message);
  } else {
    display.setCursor(0, 24);
    display.println(F("Waiting for message"));
    display.println();
    display.println(F("Topic:"));
    display.println(MQTT_TOPIC_MESSAGE);
  }
  display.display();
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  size_t n = (length > MAX_MESSAGE_LEN) ? MAX_MESSAGE_LEN : length;
  memcpy(message, payload, n);
  message[n] = '\0';
  // Printable ASCII only: the built-in font has no glyphs for anything else.
  for (size_t i = 0; i < n; i++) {
    if (message[i] != '\n' && (message[i] < 32 || message[i] > 126)) message[i] = '?';
  }
  haveMessage = (n > 0);
  messageAt = millis();
  Serial.print(F("[msg] "));
  Serial.println(message);
  redraw();
}

void connectWiFi() {
  Serial.print(F("Connecting to WiFi"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi OK, IP "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("WiFi failed, will retry in loop."));
  }
}

bool connectMqtt() {
  Serial.print(F("Connecting to MQTT... "));
  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  } else {
    ok = mqtt.connect(MQTT_CLIENT_ID);
  }
  if (ok) {
    mqtt.subscribe(MQTT_TOPIC_MESSAGE);
    Serial.print(F("OK, subscribed to "));
    Serial.println(MQTT_TOPIC_MESSAGE);
  } else {
    Serial.print(F("failed, rc="));
    Serial.println(mqtt.state());
  }
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("ESP32 SSD1306 MQTT notice board starting."));

  Wire.begin();  // ESP32 default I2C: SDA=21, SCL=22
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 not found at 0x3C - check wiring or try 0x3D."));
    while (true) {
      delay(1000);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Fluidwire notice board"));
  display.println(F("Connecting..."));
  display.display();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER);
  mqtt.setCallback(onMessage);
  connectWiFi();
}

void loop() {
  uint32_t now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnect > 10000) {
      lastReconnect = now;
      WiFi.reconnect();
    }
  } else if (!mqtt.connected()) {
    if (now - lastReconnect > 5000) {
      lastReconnect = now;
      connectMqtt();
    }
  } else {
    mqtt.loop();
  }

  // Expire a stale message so old text never looks current.
  if (haveMessage && now - messageAt > MESSAGE_TIMEOUT_MS) {
    haveMessage = false;
    Serial.println(F("[msg] timed out, back to idle screen"));
    redraw();
  }

  if (now - lastRedraw >= REDRAW_INTERVAL_MS) {
    lastRedraw = now;
    redraw();
  }
}
