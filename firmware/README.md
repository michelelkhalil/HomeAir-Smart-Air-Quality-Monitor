# HomeAir Firmware

ESP32 firmware for the HomeAir indoor air-quality monitoring prototype.

## Source

[`HomeAir.ino`](HomeAir.ino) contains the complete public firmware implementation used for this repository package.

All credentials in the public file are placeholders. Replace them locally before flashing the ESP32.

## Hardware

- ESP32 development board
- MQ-135 analog gas sensor
- SGP30 eCO₂ / TVOC sensor
- SSD1306 128×32 OLED display
- Active-low buzzer
- Green, yellow and red LEDs

## Pin Configuration

| Function | ESP32 Pin |
|---|---:|
| MQ-135 analog output | GPIO34 |
| Buzzer signal | GPIO25 |
| Green LED | GPIO17 |
| Yellow LED | GPIO18 |
| Red LED | GPIO19 |
| I²C SDA | GPIO21 |
| I²C SCL | GPIO22 |
| OLED | I²C 0x3C / 0x3D |
| SGP30 | I²C 0x58 |

## Required Arduino Libraries

- `Wire.h`
- `WiFi.h`
- `HTTPClient.h`
- `Adafruit_GFX.h`
- `Adafruit_SSD1306.h`
- `Adafruit_SGP30.h`
- `SinricPro.h`
- `SinricProDoorbell.h`

## Credentials

Never commit real Wi-Fi, Sinric Pro, or cloud credentials.

The public `HomeAir.ino` contains placeholder values only.
