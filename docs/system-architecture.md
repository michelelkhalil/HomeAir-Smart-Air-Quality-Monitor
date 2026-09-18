# HomeAir System Architecture

```mermaid
flowchart LR
    MQ[MQ-135<br/>Raw Gas Sensor] --> ESP[ESP32]
    SGP[SGP30<br/>eCO2 + TVOC] --> ESP
    ESP --> OLED[OLED Display]
    ESP --> LED[Green / Yellow / Red LEDs]
    ESP --> BUZ[Buzzer]
    ESP -->|Wi-Fi| FB[Firebase Realtime Database]
    FB --> DASH[Flutter Dashboard]
    ESP --> SINRIC[Sinric Pro]
    SINRIC --> ALEXA[Amazon Alexa / Echo]
```

## Data Flow

1. The ESP32 reads the MQ-135 analog signal and SGP30 eCO₂ / TVOC values.
2. Firmware classifies the current state as GOOD, MODERATE, or BAD.
3. The OLED, LEDs, and buzzer provide immediate local feedback.
4. Wi-Fi-connected operation pushes live and historical data to Firebase.
5. The Flutter dashboard reads cloud data for live display and history.
6. Sinric Pro links the ESP32 alert logic to Amazon Alexa.
