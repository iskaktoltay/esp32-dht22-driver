# Driver for DHT22 in ESP-IDF, written from scratch

A DHT22 temperature and humidity driver for the ESP32, written in C with ESP-IDF. I wrote it as a learning exercise without using any existing DHT library. The single-wire protocol, timing and decoding are implemented directly on top of the GPIO and timer APIs, working from the sensor's datasheet.

## How it works

- The data pin is configured as open-drain, so the ESP32 can pull the line low and release it without switching pin direction.
- The ESP32 sends the start signal (line low for 1 ms), then waits for the sensor's 80 µs low / 80 µs high response.
- Each of the 40 data bits is read by measuring how long the line stays high with `esp_timer_get_time()`: about 26–28 µs means 0, about 70 µs means 1. The driver uses 47 µs as the threshold.
- Every wait has a timeout, so a missing or disconnected sensor returns `ESP_ERR_TIMEOUT` instead of hanging.
- The checksum (low 8 bits of the sum of the first four bytes) is verified, and a mismatch returns `ESP_ERR_INVALID_CRC`.

## What I learned

**The datasheet was wrong about the data format.** It describes the 40 bits as 8-bit integer + 8-bit decimal parts for humidity and temperature. That is how the cheaper DHT11 sensor sends data. The DHT22 actually sends humidity and temperature as 16-bit values in tenths (e.g. `0x02 0x8C` = 652 = 65.2 %), with the top bit of the temperature marking a negative value.

**A checksum passing doesn't mean the value is right.** While my decoding followed the datasheet's format, every checksum still passed, because the checksum is computed on the raw bytes, not on their meaning. It only proves the data arrived intact.

**Debugging timing without a logic analyser.** I had no logic analyser, so I recorded the measured duration of every bit inside the driver and compared them with the datasheet's timings to find where reads went wrong.

## Hardware

- ESP32 dev board (ESP-WROOM-32)
- DHT22 3-pin module (ASAIR AM2302) with an on-board pull-up resistor, so no external pull-up is needed

| ESP32 | DHT22 |
|---|---|
| 3.3V | `+` |
| GND | `−` |
| GPIO4 | `out` |

## Build and run

Requires ESP-IDF v6.0.1.

```bash
idf.py set-target esp32
idf.py build flash monitor
```

Temperature and humidity are logged in tenths (e.g. `235` = 23.5 °C).

## Status and next steps

- [x] Start signal, response handshake and 40-bit read with timeouts
- [x] Checksum validation and correct 16-bit decoding, including negative temperatures
- [ ] Read every 2–3 seconds in a loop and log checksum errors instead of aborting
- [ ] Protect the timing-critical read from interrupts
- [ ] Move the driver into my air-quality monitor project as a reusable ESP-IDF component