# ESP32-SSD1306-MQTT-SUBSCRIBER-DISPLAY — show MQTT messages on an SSD1306 OLED

IoT example by [Fluidwire](https://fluidwire.com/).

The ESP32 subscribes to one MQTT topic and shows every message it receives on a 128×64 I2C SSD1306 OLED, word-wrapped to fit. It is a simple, cheap notice board: publish text from your phone, a script or Node-RED and it appears on the screen a moment later.

## What it does

- Connects to WiFi and an MQTT broker, and reconnects automatically if either drops
- Subscribes to a single topic and displays each received message, word-wrapped across up to six lines
- Hard-breaks very long words and replaces non-ASCII characters with `?` so the built-in font never shows garbage
- Shows a header with WiFi/MQTT state and how many seconds ago the message arrived
- Falls back to an idle "Waiting for message" screen after 60 seconds of silence, so stale text never looks current
- Mirrors every message to the serial monitor

## Required hardware

- 1× ESP32 dev board (DevKitC, NodeMCU-32, WROOM — any classic ESP32)
- 1× 128×64 I2C OLED with an SSD1306 controller (0.96" is the common size)
- 4 jumper wires and a USB cable

## Required libraries

Install via **Arduino Library Manager**:

- `Adafruit SSD1306` by Adafruit
- `Adafruit GFX Library` by Adafruit (pulled in automatically by SSD1306)
- `PubSubClient` by Nick O'Leary

Everything else (`WiFi`, `Wire`) ships with the **ESP32 Arduino core** (Boards Manager → `esp32` by Espressif Systems).

## Wiring

| ESP32 | OLED | Notes |
|-------|------|-------|
| 3V3 | VCC | 3.3 V |
| GND | GND | common ground |
| GPIO21 (SDA) | SDA | ESP32 default I2C data |
| GPIO22 (SCL) | SCL | ESP32 default I2C clock |

## Setup

1. Copy `config.h.example` to `config.h`.
2. Fill in your WiFi name and password, your MQTT broker, and a **unique** topic name (public brokers are shared by everyone).
3. Open `esp32-ssd1306-mqtt-subscriber-display.ino` in the Arduino IDE, choose **Tools → Board → ESP32 Dev Module**, and click **Upload**.
4. Open **Tools → Serial Monitor** at **115200** baud.
5. Send a message from any MQTT client:

```
mosquitto_pub -h broker.hivemq.com -t "fluidwire/demo/noticeboard/message" -m "Meeting at 3PM, room 2"
```

If the screen stays dark, change `OLED_ADDR` from `0x3C` to `0x3D`.

## Expected serial output

```
ESP32 SSD1306 MQTT notice board starting.
Connecting to WiFi....
WiFi OK, IP 192.168.1.42
Connecting to MQTT... OK, subscribed to fluidwire/demo/noticeboard/message
[msg] Meeting at 3PM, room 2
[msg] timed out, back to idle screen
```

## Making it yours

- **Longer or shorter timeout** — change `MESSAGE_TIMEOUT_MS` (or set it very large for a permanent board).
- **Bigger text** — call `display.setTextSize(2)` and reduce `CHARS_PER_LINE` to 10 and `BODY_LINES` to 3.
- **128×32 panel** — set `SCREEN_HEIGHT` to `32` and `BODY_LINES` to `2`.
- **Several topics** — subscribe to more topics in `connectMqtt()` and check the `topic` argument in `onMessage()`.

## Troubleshooting

- **Screen stays black and serial prints "SSD1306 not found"** — check SDA/SCL aren't swapped, confirm 3V3 and GND, and try `OLED_ADDR = 0x3D`.
- **Header says "no WiFi"** — double-check `WIFI_SSID` and `WIFI_PASSWORD`; the ESP32 only supports 2.4 GHz networks.
- **WiFi is up but MQTT shows `--`** — check `MQTT_HOST` and `MQTT_PORT`. Serial prints the PubSubClient `rc` code (`-2` means the broker could not be reached; `5` means not authorised).
- **Messages never appear** — make sure you publish to exactly the topic in `config.h`; topics are case-sensitive.
- **Won't compile, `config.h` not found** — copy `config.h.example` to `config.h` in the same folder as the sketch.

## Security note

Public brokers such as `broker.hivemq.com` are open to everyone — anybody who guesses your topic can post to your screen, and your WiFi password stays only in your local `config.h`. For anything real, use your own broker with a username/password (and TLS if possible). `config.h` is listed in `.gitignore` — never commit it.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — we ship IoT building blocks and are happy to help.
