/*
  HomeAir v3
  Smart Indoor Air Quality Monitor

  Hardware:
  - ESP32
  - MQ-135 analog gas sensor
  - SGP30 eCO2 / TVOC sensor
  - SSD1306 128x32 OLED
  - Green / Yellow / Red LEDs
  - Active-low buzzer

  Cloud / voice:
  - Firebase Realtime Database over HTTP
  - Sinric Pro / Amazon Alexa

  IMPORTANT:
  Replace placeholder credentials before running locally.
  Never commit real credentials to a public repository.
*/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SGP30.h>
#include <SinricPro.h>
#include <SinricProDoorbell.h>

// ============================================================
// CREDENTIALS - CHANGE THESE LOCALLY
// ============================================================
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASS         "YOUR_WIFI_PASSWORD"

#define SINRIC_APP_KEY    "YOUR_SINRIC_APP_KEY"
#define SINRIC_APP_SECRET "YOUR_SINRIC_APP_SECRET"
#define SINRIC_DEVICE_ID  "YOUR_SINRIC_DEVICE_ID"

// ============================================================
// Firebase Realtime Database
// ============================================================
#define FIREBASE_URL  "https://YOUR_PROJECT-default-rtdb.YOUR_REGION.firebasedatabase.app"
#define FIREBASE_ROOM "room1"

// ============================================================
// OLED
// ============================================================
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        32
#define OLED_RESET           -1
#define SDA_PIN              21
#define SCL_PIN              22
#define OLED_ADDR_PRIMARY    0x3C
#define OLED_ADDR_SECONDARY  0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledFound = false;

// ============================================================
// SGP30
// ============================================================
Adafruit_SGP30 sgp;
bool sgpFound = false;
uint16_t eCO2Value = 0;
uint16_t tvocValue = 0;

// ============================================================
// Pins
// ============================================================
#define MQ_ANALOG_PIN 34
#define BUZZER_PIN    25
#define GREEN_PIN     17
#define YELLOW_PIN    18
#define RED_PIN       19

// ============================================================
// Air-quality thresholds
// ============================================================
int goodMax       = 500;
int moderateMax   = 800;
int eCO2Threshold = 600;

// ============================================================
// Timing
// ============================================================
unsigned long readInterval       = 1000;   // sensor read every 1 second
unsigned long lastReadTime       = 0;
unsigned long firebaseInterval   = 5000;   // Firebase push every 5 seconds
unsigned long lastFirebaseTime   = 0;
unsigned long sgpCounter         = 0;

// ============================================================
// Alexa cooldown
// ============================================================
bool alexaAlertSent = false;
unsigned long alertCooldown = 60000;
unsigned long lastAlertTime = 0;

// ============================================================
// State
// ============================================================
int gasValue = 0;
int airLevel = 0;      // 0=GOOD, 1=MODERATE, 2=BAD
bool alertOn = false;
bool wifiConnected = false;

// ============================================================
// Function declarations
// ============================================================
void connectWiFi();
void setupSinricPro();
void sendAlexaAlert();
bool initOLED();
void showStartupScreen();
void updateDisplay();
bool initSGP30();
void sendToFirebase();
void printSerial();
void startupBeep();

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==================================");
  Serial.println("   HomeAir v3 - Starting up...");
  Serial.println("==================================");

  // --- LEDs ---
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);

  // Active-low LEDs: HIGH = off
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(YELLOW_PIN, HIGH);
  digitalWrite(RED_PIN, HIGH);

  // --- Buzzer ---
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  // --- I2C ---
  Wire.begin(SDA_PIN, SCL_PIN);

  // --- OLED ---
  oledFound = initOLED();
  Serial.println(oledFound ? "[OLED] Found." : "[OLED] NOT found.");

  // --- SGP30 ---
  sgpFound = initSGP30();
  if (sgpFound) {
    Serial.println("[SGP30] Found.");
  } else {
    Serial.println("[SGP30] NOT found.");
  }

  // --- Startup screen ---
  if (oledFound) showStartupScreen();

  // --- Startup beep ---
  startupBeep();

  // --- WiFi ---
  connectWiFi();

  // --- Sinric Pro ---
  if (wifiConnected) setupSinricPro();

  Serial.println("[INIT] Ready. Monitoring air quality...");
  Serial.println();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  if (wifiConnected) {
    SinricPro.handle();
  }

  unsigned long now = millis();

  // --- Read sensors every 1 second ---
  if (now - lastReadTime >= readInterval) {
    lastReadTime = now;

    // 1. MQ sensor
    gasValue = analogRead(MQ_ANALOG_PIN);

    // 2. SGP30
    if (sgpFound) {
      if (sgp.IAQmeasure()) {
        eCO2Value = sgp.eCO2;
        tvocValue = sgp.TVOC;
      }
    }

    // 3. Air-quality level
    if (gasValue <= goodMax) {
      airLevel = 0;
      alertOn = false;
    } else if (gasValue <= moderateMax) {
      airLevel = 1;
      alertOn = false;
    } else {
      airLevel = 2;
      alertOn = true;
    }

    if (sgpFound && eCO2Value > (uint16_t)eCO2Threshold) {
      airLevel = 2;
      alertOn = true;
    }

    // 4. LEDs
    digitalWrite(GREEN_PIN,  (airLevel == 0) ? LOW : HIGH);
    digitalWrite(YELLOW_PIN, (airLevel == 1) ? LOW : HIGH);
    digitalWrite(RED_PIN,    (airLevel == 2) ? LOW : HIGH);

    // 5. Buzzer
    digitalWrite(BUZZER_PIN, alertOn ? LOW : HIGH);

    // 6. Alexa alert
    if (alertOn && wifiConnected) {
      if (!alexaAlertSent || (now - lastAlertTime > alertCooldown)) {
        sendAlexaAlert();
        alexaAlertSent = true;
        lastAlertTime = now;
      }
    }

    if (!alertOn) alexaAlertSent = false;

    // 7. Serial
    printSerial();

    // 8. OLED
    if (oledFound) updateDisplay();

    // 9. SGP30 baseline
    sgpCounter++;
    if (sgpFound && sgpCounter == 30) {
      uint16_t eCO2_base, TVOC_base;
      if (sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
        Serial.print("[SGP30] Baseline - eCO2: 0x");
        Serial.print(eCO2_base, HEX);
        Serial.print("  TVOC: 0x");
        Serial.println(TVOC_base, HEX);
      }
    }
  }

  // --- Push to Firebase every 5 seconds ---
  if (wifiConnected && now - lastFirebaseTime >= firebaseInterval) {
    lastFirebaseTime = now;
    sendToFirebase();
  }
}

// ============================================================
// FIREBASE - HTTP PUT/POST to Realtime Database
// ============================================================
void sendToFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Build JSON payload
  String json = "{";
  json += "\"gasRaw\":" + String(gasValue) + ",";
  json += "\"co2ppm\":" + String(eCO2Value) + ",";
  json += "\"tvoc\":" + String(tvocValue) + ",";
  json += "\"airLevel\":" + String(airLevel) + ",";
  json += "\"timestamp\":{\".sv\":\"timestamp\"}";
  json += "}";

  HTTPClient http;

  // 1. PUT to /latest (overwrites current reading)
  String latestUrl = String(FIREBASE_URL) + "/rooms/" + FIREBASE_ROOM + "/latest.json";
  http.begin(latestUrl);
  http.addHeader("Content-Type", "application/json");

  int code1 = http.PUT(json);
  if (code1 > 0) {
    Serial.print("[Firebase] latest -> ");
    Serial.println(code1);
  } else {
    Serial.print("[Firebase] latest FAILED: ");
    Serial.println(http.errorToString(code1));
  }
  http.end();

  // 2. POST to /readings (appends to history with auto-key)
  String historyUrl = String(FIREBASE_URL) + "/rooms/" + FIREBASE_ROOM + "/readings.json";
  http.begin(historyUrl);
  http.addHeader("Content-Type", "application/json");

  int code2 = http.POST(json);
  if (code2 > 0) {
    Serial.print("[Firebase] history -> ");
    Serial.println(code2);
  } else {
    Serial.print("[Firebase] history FAILED: ");
    Serial.println(http.errorToString(code2));
  }
  http.end();
}

// ============================================================
// WIFI
// ============================================================
void connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_SSID);

  if (oledFound) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("WiFi...");
    display.display();
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println(" Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());

    if (oledFound) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("WiFi OK");
      display.setCursor(0, 12);
      display.print(WiFi.localIP());
      display.display();
      delay(1500);
    }
  } else {
    wifiConnected = false;
    Serial.println(" FAILED - running offline.");

    if (oledFound) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("WiFi FAILED");
      display.setCursor(0, 12);
      display.print("Running offline");
      display.display();
      delay(1500);
    }
  }
}

// ============================================================
// SINRIC PRO (Alexa)
// ============================================================
void setupSinricPro() {
  SinricProDoorbell &doorbell = SinricPro[SINRIC_DEVICE_ID];
  SinricPro.begin(SINRIC_APP_KEY, SINRIC_APP_SECRET);
  Serial.println("[Alexa] Sinric Pro connected.");
}

void sendAlexaAlert() {
  SinricProDoorbell &doorbell = SinricPro[SINRIC_DEVICE_ID];
  doorbell.sendDoorbellEvent();
  Serial.println("[Alexa] ** AIR QUALITY ALERT sent **");
}

// ============================================================
// OLED
// ============================================================
bool initOLED() {
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_PRIMARY)) return true;
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_SECONDARY)) return true;
  return false;
}

void showStartupScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(34, 2);
  display.println("HOMEAIR");
  display.setCursor(31, 18);
  display.println("SYSTEM OK");
  display.display();
  delay(2000);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Gas:");
  display.print(gasValue);

  if (sgpFound) {
    display.setCursor(72, 0);
    display.print("CO2:");
    display.print(eCO2Value);
  }

  display.setCursor(0, 12);
  if (sgpFound) {
    display.print("TVOC:");
    display.print(tvocValue);
    display.print(" ppb");
  } else {
    display.print("HOMEAIR");
  }

  const char* labels[] = {"GOOD", "MODERATE", "BAD"};
  display.setCursor(0, 24);
  display.print(labels[airLevel]);

  display.setCursor(100, 24);
  display.print(wifiConnected ? "W" : "");

  display.display();
}

// ============================================================
// SGP30
// ============================================================
bool initSGP30() {
  if (sgp.begin()) {
    Serial.print("[SGP30] Serial #: ");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
    return true;
  }
  return false;
}

// ============================================================
// SERIAL
// ============================================================
void printSerial() {
  Serial.print("MQ: ");
  Serial.print(gasValue);

  if (sgpFound) {
    Serial.print(" | eCO2: ");
    Serial.print(eCO2Value);
    Serial.print(" ppm | TVOC: ");
    Serial.print(tvocValue);
    Serial.print(" ppb");
  }

  const char* labels[] = {"GOOD", "MODERATE", "BAD"};
  Serial.print(" [");
  Serial.print(labels[airLevel]);
  Serial.println("]");
}

// ============================================================
// BUZZER
// ============================================================
void startupBeep() {
  digitalWrite(BUZZER_PIN, LOW);
  delay(80);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(80);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("[BUZZ] Startup beep done.");
}
