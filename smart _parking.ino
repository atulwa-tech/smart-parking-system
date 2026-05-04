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
//  ██ RFID RC522 (SPI):
//    ├─ SDA (SS)  → GPIO 5
//    ├─ SCK       → GPIO 18
//    ├─ MOSI      → GPIO 23
//    ├─ MISO      → GPIO 19
//    ├─ RST       → GPIO 22
//    ├─ GND       → GND
//    └─ 3.3V      → 3.3V
//
//  Servo Motor (single shared gate):
//    Signal  → GPIO 13
//    VCC     → 5V (use external supply if servo jitters)
//    GND     → GND (common with ESP32)
//
//  ██ IMPORTANT HARDWARE:
//    - RFID MUST be 3.3V (NOT 5V)
//    - Servo needs external 5V supply
//    - Keep wires short, no loose breadboard
//
//  LCD (I2C):
//    SDA → GPIO 25   ← UPDATED
//    SCL → GPIO 26   ← UPDATED
//    VCC → 5V
//    GND → GND
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>                     // ← ADDED: needed for custom I2C pins
#include <LiquidCrystal_I2C.h>

// ── WiFi Credentials ────────────────────────────────────────
const char* WIFI_SSID     = "rupesh009";
const char* WIFI_PASSWORD = "rupesh009";

// ── Backend Server ───────────────────────────────────────────
const char* SERVER_IP = "http://10.231.49.14:3000";

// ── Pin Definitions ──────────────────────────────────────────
// ── RFID (RC522) – SPI Configuration ──────────────────────────
#define RFID_SCK_PIN  18  // SPI Clock
#define RFID_MOSI_PIN 23  // SPI Master Out Slave In
#define RFID_MISO_PIN 19  // SPI Master In Slave Out
#define RFID_SS_PIN   5   // Chip Select / SDA (FIXED: was 21, conflicted with I2C)
#define RFID_RST_PIN  22  // Reset

#define SERVO_PIN     13   // Single shared gate servo (FIXED: was 14)
#define LCD_SDA_PIN   25   // I2C SDA
#define LCD_SCL_PIN   26   // I2C SCL

// ── LED Status Indicators ──────────────────────────────────
#define LED_RED       2   // Red: Gate closed (servo off)
#define LED_YELLOW    4   // Yellow: Gate closing
#define LED_GREEN     15  // Green: Gate open (FIXED: was 5, now used for RFID SS)

// ── Objects ──────────────────────────────────────────────────
MFRC522           rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo             gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Address 0x27, 16 cols, 2 rows

// ── Gate Config ──────────────────────────────────────────────
const int GATE_OPEN    = 90;   // degrees — adjust if your servo differs
const int GATE_CLOSED  = 0;    // degrees
const int GATE_HOLD_MS = 5000; // gate stays open 5 seconds
// ── RFID Tags (Update with your actual card UIDs) ──────────────────────
String tag1 = "52 74 5C 5C";  // Your card 1
String tag2 = "BF 29 70 CB";  // Your card 2
String tag3 = "8F F6 78 CB";  // Your card 3
String tag4 = "62 68 4 5C";   // Your card 4
// ── Global State ─────────────────────────────────────────────
int  permFree  = 4;
int  visitFree = 4;
int  lastAssignedSlot = 0;
unsigned long cardDetectTime = 0;
String lastCardUID = "";
bool  isEntryMode = true;
bool  isPermSlot = false;  // ← Track if last assigned slot is permanent
// ── Timing ───────────────────────────────────────────────────
unsigned long lastFetch        = 0;
unsigned long lastScreenUpdate = 0;
unsigned long gateCloseTime    = 0;
unsigned long gateClosingStartTime = 0;  // ← Track when gate starts closing
bool          isGateOpen       = false;
const int     CLOSING_DURATION_MS = 800;  // Gate servo takes ~800ms to close

// ── Forward Declarations ─────────────────────────────────────
void lcdPrint(String line1, String line2);
void showStatusScreen();
void openGate();
void closeGate();
void connectWiFi();
void reconnectWiFi();
void fetchSlotStatus();
void checkGateCommand();
void notifyBackendSlotOccupied(String uid);
void notifyBackendExit(String uid);

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // ── I2C init on custom pins ────────────────────────────────
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);  // ← KEY CHANGE: GPIO 25 = SDA, GPIO 26 = SCL
  delay(100);

  // ── LCD init ───────────────────────────────────────────────
  lcd.init();
  lcd.backlight();
  lcdPrint("SmartPark v1.0", "Initializing...");
  Serial.println("\n=== SmartPark Booting ===");

  // ── SPI + RFID ────────────────────────────────────────────
  Serial.println("\n[SPI] Initializing SPI bus...");
  // FIXED: SPI.begin(SCK, MISO, MOSI, SS) - correct order for ESP32
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  delay(100);

  Serial.println("[RFID] Initializing MFRC522...");
  rfid.PCD_Init();
  rfid.PCD_AntennaOn();
  delay(50);

  byte version = 0;
  for (int attempt = 0; attempt < 5; attempt++) {  // ← INCREASED: more retry attempts
    version = rfid.PCD_ReadRegister(rfid.VersionReg);
    Serial.print("[Attempt ");
    Serial.print(attempt + 1);
    Serial.print("] RFID Version: 0x");
    Serial.println(version, HEX);
    if (version != 0x00 && version != 0xFF) {
      break;
    }
    delay(150);  // ← INCREASED: longer delay between attempts
  }

  if (version == 0x00 || version == 0xFF) {
    Serial.println("\n✗✗✗ RFID NOT DETECTED ✗✗✗");
    Serial.println("TROUBLESHOOTING:");
    Serial.println("  1. Check SPI wiring: SCK(18), MOSI(23), MISO(19), SS(21)");
    Serial.println("  2. Verify RST pin connected to GPIO 22");
    Serial.println("  3. Ensure MFRC522 has 3.3V power (NOT 5V)");
    Serial.println("  4. Try a different USB cable (power issue)");
    Serial.println("  5. Swap MISO/MOSI if unsure");
    lcdPrint("RFID ERROR!", "Check wiring");
    for (int i = 0; i < 10; i++) {
      lcd.noBacklight(); delay(300);
      lcd.backlight();   delay(300);
    }
  } else {
    Serial.println("\n✓✓✓ RFID DETECTED ✓✓✓");
    Serial.print("Firmware version: 0x");
    Serial.println(version, HEX);

    Serial.println("[RFID] Configuring antenna gain...");
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    delay(50);

    Serial.println("[RFID] Running self-test...");
    byte selfTest = rfid.PCD_PerformSelfTest();
    if (selfTest) {
      Serial.println("✓ RFID Self-Test PASSED");
    } else {
      Serial.println("✗ RFID Self-Test FAILED (may still work)");
    }

    Serial.println("✓ RFID initialization complete - Ready to scan cards");
    lcdPrint("RFID OK", "v0x" + String(version, HEX));
    delay(1000);
  }

  // ── LED initialization ────────────────────────────────────
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_RED, HIGH);      // Red on initially (gate closed)
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);
  delay(100);

  // ── Single Servo: attach and close gate ───────────────────
  gateServo.attach(SERVO_PIN);
  gateServo.write(GATE_CLOSED);
  isGateOpen = false;
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
void loop() {

  // ── Auto-close gate after hold time ──────────────────────
  if (isGateOpen && millis() >= gateCloseTime) {
    closeGate();
    lcdPrint("Gate Closed", "Drive safely!");
    lastScreenUpdate = millis() + 1500;
  }

  // ── LED management: turn red on when gate finishes closing ──
  if (gateClosingStartTime > 0 && millis() - gateClosingStartTime > CLOSING_DURATION_MS) {
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
    gateClosingStartTime = 0;
  }

  // ── RFID check — continuous polling ──────────────────────
  if (rfid.PICC_IsNewCardPresent()) {
    if (!rfid.PICC_ReadCardSerial()) return;

    // ── READ REAL UID ──────────────────────────────────────────
    String readTag = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) readTag += "0";
      readTag += String(rfid.uid.uidByte[i], HEX);
      if (i != rfid.uid.size - 1) readTag += " ";
    }
    readTag.toUpperCase();
    Serial.print("UID: ");
    Serial.println(readTag);

    Serial.println("[RFID] Card detected! Processing...");

    if (!isEntryMode && lastCardUID != "") {
      // ── Same card detected again → Assign SECOND slot instead of releasing ──
      Serial.println("[MULTI-SLOT] Same card detected again - assigning second slot");
      Serial.print("Card UID: ");
      Serial.println(lastCardUID);

      // Try to assign a second slot (permanent first, then visitor)
      if (permFree > 0) {
        int secondSlot = 5 - permFree;
        String secondUID = "PERM_SLOT_" + String(secondSlot);

        Serial.println("──────────────────────");
        Serial.print("✓ Second permanent slot assigned: ");
        Serial.println(secondSlot);

        notifyBackendSlotOccupied(secondUID);

        openGate();
        lcdPrint("Perm Slot " + String(secondSlot), "Assigned!");

        permFree--;
        Serial.print("Permanent slots remaining: ");
        Serial.println(permFree);

        delay(1500);
      } else if (visitFree > 0) {
        int secondSlot = 5 - visitFree;
        String secondUID = "VISIT_SLOT_" + String(secondSlot);

        Serial.println("──────────────────────");
        Serial.print("✓ Second visitor slot assigned: ");
        Serial.println(secondSlot);

        notifyBackendSlotOccupied(secondUID);

        openGate();
        lcdPrint("Visit Slot " + String(secondSlot), "Assigned!");

        visitFree--;
        Serial.print("Visitor slots remaining: ");
        Serial.println(visitFree);

        delay(1500);
      } else {
        Serial.println("✗ No slots available for second booking");
        lcdPrint("FULL!", "No second slot");
        lastScreenUpdate = millis() + 2000;
        delay(1000);
      }

      // ← CRITICAL FIX: Reset state after assigning second slot
      isEntryMode = true;  // Allow next card scan to assign first slot for that card
      lastCardUID = "";
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      delay(500);

    } else if (isEntryMode) {
      // ── CHECK IF TAG IS AUTHORIZED ───────────────────────────
      if (readTag == tag1 || readTag == tag2 || readTag == tag3 || readTag == tag4) {
        if (permFree > 0) {
        lastAssignedSlot = 5 - permFree;
        lastCardUID = "PERM_SLOT_" + String(lastAssignedSlot);
        isPermSlot = true;  // ← Mark as permanent slot

        Serial.println("─────────────────────────");
        Serial.print("✓ Card assigned to Permanent Slot: ");
        Serial.println(lastAssignedSlot);

        notifyBackendSlotOccupied(lastCardUID);

        openGate();
        lcdPrint("Perm Slot " + String(lastAssignedSlot), "Welcome!");

        permFree--;
        Serial.print("Permanent slots remaining: ");
        Serial.println(permFree);

          isEntryMode = false;
          delay(1500);

        } else {
          if (visitFree > 0) {
          int visitorSlot = 5 - visitFree;
          lastCardUID = "VISIT_SLOT_" + String(visitorSlot);
          isPermSlot = false;  // ← Mark as visitor slot

          Serial.println("─────────────────────────");
          Serial.print("✓ Card assigned to Visitor Slot: ");
          Serial.println(visitorSlot);

          notifyBackendSlotOccupied(lastCardUID);

          openGate();
          lcdPrint("Visit Slot " + String(visitorSlot), "Welcome!");

          visitFree--;
          Serial.print("Visitor slots remaining: ");
          Serial.println(visitFree);

            isEntryMode = false;
            delay(1500);

          } else {
            Serial.println("✗ Parking lot FULL - all slots occupied");
            lcdPrint("FULL!", "Try later");
            lastScreenUpdate = millis() + 2000;
            delay(1000);
          }
        }
      } else {
        Serial.println("✗ UNAUTHORIZED TAG!");
        lcdPrint("Access Denied", "Invalid card");
        lastScreenUpdate = millis() + 2000;
        delay(1000);
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      delay(500);
    }
  }

  // ── Periodic slot refresh every 15 seconds ───────────────
  if (millis() - lastFetch > 15000) {
    fetchSlotStatus();
    lastFetch = millis();
  }

  // ── Check for gate commands from frontend/backend every 2 seconds ──
  static unsigned long lastGateCheck = 0;
  if (millis() - lastGateCheck > 2000) {
    checkGateCommand();
    lastGateCheck = millis();
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

// ── POST /rfid → Entry: Assign slot ───────────────────────
void notifyBackendSlotOccupied(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected - slot update queued locally");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_IP) + "/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String payload = "{\"uid\":\"" + uid + "\"}";
  Serial.print("→ ENTRY: Sending to backend ");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.print("Backend response: HTTP ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✓ Slot occupied: " + response);
  } else {
    Serial.print("⚠ Backend failed: HTTP ");
    Serial.println(httpCode);
  }

  http.end();
}

// ── POST /rfid → Exit: Release slot ────────────────────────
void notifyBackendExit(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected - exit queued locally");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_IP) + "/rfid";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String payload = "{\"uid\":\"" + uid + "\"}";
  Serial.print("→ EXIT: Sending to backend ");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.print("Backend response: HTTP ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✓ Slot released: " + response);
    fetchSlotStatus();
    lastFetch = millis();
  } else {
    Serial.print("⚠ Backend failed: HTTP ");
    Serial.println(httpCode);
  }

  http.end();
}

// ── GET /gate → Check if frontend booked a visitor slot ──────────────────────
void checkGateCommand() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(SERVER_IP) + "/gate");
  http.setTimeout(3000);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Gate response: " + response);

    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, response)) {
      bool shouldOpen = doc["open"] | false;
      String reason = doc["reason"] | "";

      if (shouldOpen && !isGateOpen) {
        Serial.println("✓ Frontend triggered gate: " + reason);
        openGate();
        lcdPrint("Visitor Slot", "Welcome!");
        lastScreenUpdate = millis() + 1500;
      }
    }
  }
  http.end();
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
  // LED: Turn off red and yellow, turn on green
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, HIGH);
  Serial.println("Gate OPENED (auto-close in " + String(GATE_HOLD_MS / 1000) + "s)");
}

void closeGate() {
  gateServo.write(GATE_CLOSED);
  isGateOpen = false;
  gateClosingStartTime = millis();  // Start tracking closing duration
  // LED: Turn off green, turn on yellow (closing indicator)
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH);
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