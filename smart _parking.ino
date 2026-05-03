// ============================================================
//  Smart Parking System — ESP32 Arduino Code
//  Single Servo Gate Version
// ============================================================
//  Components:
//    - MFRC522 RFID Reader
//    - 1x Servo Motor (shared gate for all slots)
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
//
//  Servo Motor (single shared gate):
//    Signal  → GPIO 13
//    VCC     → 5V (use external supply if servo jitters)
//    GND     → GND (common with ESP32)
//
//  LCD (I2C):
//    SDA → GPIO 21
//    SCL → GPIO 22
//    VCC → 5V
//    GND → GND
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// ── WiFi Credentials ────────────────────────────────────────
const char* WIFI_SSID     = "rupesh009";
const char* WIFI_PASSWORD = "rupesh009";

// ── Backend Server ───────────────────────────────────────────
const char* SERVER_IP = "http://10.231.49.14:3000";

// ── Pin Definitions ──────────────────────────────────────────
#define RFID_SS_PIN   5
#define RFID_RST_PIN  4
#define SERVO_PIN     13   // Single shared gate servo

// ── Objects ──────────────────────────────────────────────────
MFRC522           rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo             gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Gate Config ──────────────────────────────────────────────
const int GATE_OPEN    = 90;   // degrees — adjust if your servo differs
const int GATE_CLOSED  = 0;    // degrees
const int GATE_HOLD_MS = 5000; // gate stays open 5 seconds

// ── Global State ─────────────────────────────────────────────
int  permFree  = 4;
int  visitFree = 4;

// ── Timing ───────────────────────────────────────────────────
unsigned long lastFetch        = 0;
unsigned long lastScreenUpdate = 0;
unsigned long gateCloseTime    = 0;
unsigned long lastRFIDDebug    = 0;  // For diagnostic output
bool          isGateOpen       = false;

// ── Forward Declarations ─────────────────────────────────────
void lcdPrint(String line1, String line2);
void showStatusScreen();
void openGate();
void closeGate();
void connectWiFi();
void reconnectWiFi();
void fetchSlotStatus();
int  sendRFIDToServer(String uid);
String getUID();

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // ── LCD init ───────────────────────────────────────────────
  lcd.init();
  lcd.backlight();
  lcdPrint("SmartPark v1.0", "Initializing...");
  Serial.println("\n=== SmartPark Booting ===");

  // ── SPI + RFID ────────────────────────────────────────────
  SPI.begin(18, 19, 23, 5);
  
  // Initialize RFID with proper reset sequence
  pinMode(RFID_RST_PIN, OUTPUT);
  digitalWrite(RFID_RST_PIN, HIGH);
  delay(50);
  digitalWrite(RFID_RST_PIN, LOW);
  delay(100);
  digitalWrite(RFID_RST_PIN, HIGH);
  delay(200);
  
  rfid.PCD_Init();
  delay(200);

  // Boost antenna for better card detection range
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);

  // Verify RFID chip is responding (retry up to 3 times)
  byte version = 0;
  for (int i = 0; i < 3; i++) {
    version = rfid.PCD_ReadRegister(rfid.VersionReg);
    Serial.print("RFID Firmware Check #");
    Serial.print(i + 1);
    Serial.print(": 0x");
    Serial.println(version, HEX);
    if (version != 0x00 && version != 0xFF) break;
    delay(100);
  }

  if (version == 0x00 || version == 0xFF) {
    Serial.println("\n❌ ERROR: RFID module not detected!");
    Serial.println("   • Check SPI wiring: SCK=18, MOSI=23, MISO=19, SS=5");
    Serial.println("   • Check RST pin: 4 → must be connected");
    Serial.println("   • Check power: RFID needs 3.3V, min 100mA");
    Serial.println("   • Try: Remove RST cap (if present), power cycle module");
    lcdPrint("RFID ERROR!", "Check wiring");
    for (int i = 0; i < 6; i++) {
      lcd.noBacklight(); delay(300);
      lcd.backlight();   delay(300);
    }
  } else {
    Serial.print("✓ RFID OK - Firmware: 0x");
    Serial.println(version, HEX);
    lcdPrint("RFID OK", "v0x" + String(version, HEX));
    delay(1000);
  }

  // ── Single Servo: attach and close gate ───────────────────
  gateServo.attach(SERVO_PIN);
  closeGate();
  delay(500);
  Serial.println("Gate initialized (closed)");

  // ── WiFi ──────────────────────────────────────────────────
  connectWiFi();

  // ── Initial slot fetch ────────────────────────────────────
  fetchSlotStatus();
  lastFetch = millis();

  Serial.println("=== Ready. Waiting for cards... ===\n");
}

// ============================================================
unsigned long lastRFIDDebug = 0;  // Debug timing

void loop() {

  // ── Auto-close gate after hold time ──────────────────────
  if (isGateOpen && millis() >= gateCloseTime) {
    closeGate();
    lcdPrint("Gate Closed", "Drive safely!");
    lastScreenUpdate = millis() + 1500;
  }

  // ── RFID Debug output every 3 seconds ───────────────────
  if (millis() - lastRFIDDebug > 3000) {
    bool cardPresent = rfid.PICC_IsNewCardPresent();
    Serial.print("RFID Status: Card Present = ");
    Serial.print(cardPresent ? "TRUE" : "FALSE");
    Serial.print(" | Antenna OK = ");
    Serial.println((rfid.PCD_ReadRegister(rfid.ComIrqReg) & 0x10) ? "YES" : "NO");
    lastRFIDDebug = millis();
  }

  // ── RFID check — never blocked ───────────────────────────
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    String uid = getUID();
    Serial.println("─────────────────────────");
    Serial.print("Card UID: ");
    Serial.println(uid);

    lcdPrint("Card Scanned:", uid.substring(0, 16));
    delay(300);

    // If gate is already open, don't scan again
    if (isGateOpen) {
      lcdPrint("Gate already", "open! Wait...");
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      lastScreenUpdate = millis() + 2000;
      return;
    }

    int result = sendRFIDToServer(uid);

    if (result > 0) {
      // Valid card — open the shared gate
      openGate();
      lcdPrint("Slot " + String(result) + " Assigned", "Welcome!");

    } else if (result == -2) {
      // Exit — open gate for departure
      openGate();
      lcdPrint("Goodbye!", "Gate Opening...");

    } else {
      // Unregistered or error
      lcdPrint("Access DENIED", "Unknown Card");
      lastScreenUpdate = millis() + 2000;
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    fetchSlotStatus();
    lastFetch = millis();
  }

  // ── Periodic slot refresh every 15 seconds ───────────────
  if (millis() - lastFetch > 15000) {
    fetchSlotStatus();
    lastFetch = millis();
  }

  // ── LCD status screen refresh every 2 seconds ────────────
  if (!isGateOpen && millis() - lastScreenUpdate > 2000) {
    showStatusScreen();
    lastScreenUpdate = millis();
  }

  // ── Reconnect WiFi if dropped ────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
}

// ── Read RFID UID as uppercase hex string ────────────────────
String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ── POST /rfid → get result from server ──────────────────────
// Returns:  1-4  → valid slot assigned (entry)
//           -2   → exit event
//           -1   → denied or error
int sendRFIDToServer(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    lcdPrint("No WiFi!", "Card ignored");
    lastScreenUpdate = millis() + 1500;
    return -1;
  }

  HTTPClient http;
  String url = String(SERVER_IP) + "/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String payload  = "{\"uid\":\"" + uid + "\"}";
  int    httpCode = http.POST(payload);

  Serial.print("POST /rfid → HTTP ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Response: " + response);

    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, response);
    http.end();

    if (err) {
      Serial.println("JSON parse error");
      return -1;
    }

    bool   success = doc["success"] | false;
    String action  = doc["action"]  | "";
    int    slot    = doc["slot"]    | 0;

    if (!success) return -1;
    if (action == "exit") return -2;
    return slot;

  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    lcdPrint("Server Error", "Code:" + String(httpCode));
    lastScreenUpdate = millis() + 2000;
    http.end();
    return -1;
  }
}

// ── GET /slots → refresh display counts ──────────────────────
void fetchSlotStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(SERVER_IP) + "/slots");
  http.setTimeout(5000);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    if (!deserializeJson(doc, response)) {
      JsonObject stats = doc["stats"];
      permFree  = stats["permanentFree"] | permFree;
      visitFree = stats["visitorFree"]   | visitFree;
      Serial.print("Slots → Perm: ");
      Serial.print(permFree);
      Serial.print("  Visitor: ");
      Serial.println(visitFree);
    }
  } else {
    Serial.print("Slot fetch failed: HTTP ");
    Serial.println(httpCode);
  }
  http.end();
}

// ── LCD Helpers ───────────────────────────────────────────────
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
  lcd.print("Free:");
  lcd.print(permFree + visitFree);
  lcd.print("  Scan card");
}

// ── Single Gate Control ───────────────────────────────────────
void openGate() {
  gateServo.write(GATE_OPEN);
  isGateOpen    = true;
  gateCloseTime = millis() + GATE_HOLD_MS;
  Serial.println("Gate OPENED (auto-close in " + String(GATE_HOLD_MS / 1000) + "s)");
}

void closeGate() {
  gateServo.write(GATE_CLOSED);
  isGateOpen = false;
  Serial.println("Gate CLOSED");
}

// ── WiFi ──────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  lcdPrint("Connecting WiFi", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
    lcdPrint("WiFi Connected!", WiFi.localIP().toString());
    delay(1500);
  } else {
    Serial.println("\nWiFi FAILED. Offline mode.");
    lcdPrint("WiFi FAILED", "Offline mode");
    delay(2000);
  }
}

void reconnectWiFi() {
  WiFi.reconnect();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi reconnected: " + WiFi.localIP().toString());
  }
}

// ============================================================
//  Libraries (Arduino Library Manager):
//  - MFRC522            by GithubCommunity
//  - ESP32Servo         by Kevin Harrington
//  - LiquidCrystal_I2C  by Frank de Brabander
//  - ArduinoJson        by Benoit Blanchon
// ============================================================