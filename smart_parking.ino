// ============================================================
//  Smart Parking System — ESP32 Arduino Code
// ============================================================
//  Components:
//    - MFRC522 RFID Reader
//    - 4x Servo Motors (one per permanent slot gate)
//    - 16x2 LCD with I2C backpack
//    - WiFi
//
//  Pin Connections:
//  ─────────────────────────────────────────────────────────
//  MFRC522 (SPI):
//    SDA/SS  → GPIO 5
//    SCK     → GPIO 18
//    MOSI    → GPIO 23
//    MISO    → GPIO 19
//    RST     → GPIO 4
//    GND     → GND
//    3.3V    → 3.3V
//
//  Servo Motors:
//    Slot 1 Gate → GPIO 13
//    Slot 2 Gate → GPIO 12
//    Slot 3 Gate → GPIO 14
//    Slot 4 Gate → GPIO 27
//
//  LCD (I2C):
//    SDA → GPIO 21
//    SCL → GPIO 22
//    VCC → 5V
//    GND → GND
//    I2C Address: 0x27 (change to 0x3F if needed)
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// ── WiFi Credentials ────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ── Backend Server ───────────────────────────────────────────
// Replace with your server's local IP (run `ipconfig` / `ifconfig` on your PC)
const char* SERVER_IP   = "http://192.168.1.100:3000";

// ── Pin Definitions ──────────────────────────────────────────
#define RFID_SS_PIN   5
#define RFID_RST_PIN  4

#define SERVO_SLOT1   13
#define SERVO_SLOT2   12
#define SERVO_SLOT3   14
#define SERVO_SLOT4   27

// ── Objects ──────────────────────────────────────────────────
MFRC522             rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo               servo[4];
LiquidCrystal_I2C   lcd(0x27, 16, 2);

// ── Servo Angle Config ───────────────────────────────────────
const int GATE_OPEN   = 90;   // degrees
const int GATE_CLOSED = 0;    // degrees
const int GATE_HOLD_MS = 5000; // gate stays open 5 seconds

// ── State ────────────────────────────────────────────────────
int  permFree  = 4;
int  visitFree = 4;

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcdPrint("SmartPark v1.0", "Initializing...");

  // SPI + RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID reader initialized");

  // Servos — attach and close all gates
  int servoPins[4] = {SERVO_SLOT1, SERVO_SLOT2, SERVO_SLOT3, SERVO_SLOT4};
  for (int i = 0; i < 4; i++) {
    servo[i].attach(servoPins[i]);
    closeGate(i);
    delay(200);
  }

  // WiFi
  connectWiFi();

  // Initial slot fetch
  fetchSlotStatus();
  delay(500);
}

// ── Main Loop ────────────────────────────────────────────────
void loop() {

  // ── RFID Check ──────────────────────────────────────────
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    String uid = getUID();
    Serial.print("Card detected. UID: ");
    Serial.println(uid);

    lcdPrint("Card Scanned:", uid.substring(0,16));
    delay(500);

    // Send UID to backend
    int slot = sendRFIDToServer(uid);

    if (slot > 0) {
      // Valid card: open corresponding gate
      int idx = slot - 1;          // slot 1→index 0
      openGate(idx);

      String line1 = "Slot " + String(slot) + " -> OPEN";
      lcdPrint(line1, "Welcome!");
      delay(GATE_HOLD_MS);
      closeGate(idx);
      lcdPrint("Gate Closed", "Drive safely!");
      delay(1500);

    } else if (slot == -2) {
      // Exit detected (toggle off)
      lcdPrint("Goodbye!", "Slot released");
      delay(2000);

    } else {
      // Unregistered card
      lcdPrint("Access DENIED", "Unknown Card");
      delay(2000);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    // Refresh slot status after RFID event
    fetchSlotStatus();
  }

  // ── Periodic slot status refresh (every 15 s) ──────────
  static unsigned long lastFetch = 0;
  if (millis() - lastFetch > 15000) {
    fetchSlotStatus();
    lastFetch = millis();
  }

  // ── Show default status screen ─────────────────────────
  showStatusScreen();
  delay(2000);
}

// ── Read RFID UID as hex string ──────────────────────────────
String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ── POST /rfid → returns slot number ────────────────────────
// Returns: slot number (1-4) on success
//          -1 on error / unregistered
//          -2 on exit (toggle)
int sendRFIDToServer(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return -1;
  }

  HTTPClient http;
  String url = String(SERVER_IP) + "/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"uid\":\"" + uid + "\"}";
  int httpCode   = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("RFID response: " + response);

    DynamicJsonDocument doc(256);
    deserializeJson(doc, response);

    bool   success = doc["success"];
    String action  = doc["action"] | "";
    int    slot    = doc["slot"] | 0;

    http.end();

    if (!success) return -1;
    if (action == "exit") return -2;
    return slot;

  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    http.end();
    return -1;
  }
}

// ── GET /slots → refresh local stats ────────────────────────
void fetchSlotStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
    return;
  }

  HTTPClient http;
  String url = String(SERVER_IP) + "/slots";
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    JsonObject stats = doc["stats"];
    permFree  = stats["permanentFree"] | 4;
    visitFree = stats["visitorFree"]   | 4;

    Serial.print("Slots updated — Perm free: ");
    Serial.print(permFree);
    Serial.print("  Visitor free: ");
    Serial.println(visitFree);
  } else {
    Serial.print("Fetch failed, HTTP: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ── Display Helpers ──────────────────────────────────────────
void lcdPrint(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1); lcd.print(line2.substring(0, 16));
}

void showStatusScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("P:");
  lcd.print(permFree);
  lcd.print("/4 V:");
  lcd.print(visitFree);
  lcd.print("/4");
  lcd.setCursor(0, 1);
  int total = permFree + visitFree;
  lcd.print("Free: ");
  lcd.print(total);
  lcd.print("  Scan card");
}

// ── Servo Gate Control ───────────────────────────────────────
void openGate(int idx) {
  if (idx < 0 || idx > 3) return;
  servo[idx].write(GATE_OPEN);
  Serial.print("Gate ");
  Serial.print(idx + 1);
  Serial.println(" OPENED");
}

void closeGate(int idx) {
  if (idx < 0 || idx > 3) return;
  servo[idx].write(GATE_CLOSED);
  Serial.print("Gate ");
  Serial.print(idx + 1);
  Serial.println(" CLOSED");
}

// ── WiFi ─────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  lcdPrint("Connecting WiFi", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    lcdPrint("WiFi Connected!", WiFi.localIP().toString());
    delay(1500);
  } else {
    Serial.println("\nWiFi FAILED. Running offline.");
    lcdPrint("WiFi FAILED", "Offline mode");
    delay(2000);
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
    WiFi.reconnect();
    delay(3000);
  }
}

// ============================================================
//  Required Libraries (install via Arduino Library Manager):
//  - MFRC522 by GithubCommunity
//  - ESP32Servo by Kevin Harrington
//  - LiquidCrystal_I2C by Frank de Brabander
//  - ArduinoJson by Benoit Blanchon
// ============================================================
