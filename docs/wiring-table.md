# HomeAir Wiring Table

| Component / Signal | ESP32 Connection | Notes |
|---|---|---|
| MQ-135 analog output | GPIO34 | ADC-capable input |
| SGP30 SDA | GPIO21 | Shared I²C bus |
| SGP30 SCL | GPIO22 | Shared I²C bus |
| OLED SDA | GPIO21 | Shared I²C bus |
| OLED SCL | GPIO22 | Shared I²C bus |
| OLED address | 0x3C / 0x3D | Firmware supports either address |
| SGP30 address | 0x58 | Fixed I²C address |
| Buzzer signal | GPIO25 | Active-low in prototype |
| Green LED | GPIO17 | Active-low |
| Yellow LED | GPIO18 | Active-low |
| Red LED | GPIO19 | Active-low |

## Notes

- Confirm each breakout board's supply requirements before rebuilding the circuit.
- The prototype LEDs and buzzer use active-low logic.
- The MQ-135 is used as a raw analog gas indicator rather than a calibrated CO₂ instrument.
- The SGP30 provides digital eCO₂ and TVOC measurements.
