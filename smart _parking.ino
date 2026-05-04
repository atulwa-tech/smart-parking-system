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
//    SDA/SS  → GPIO 21
//    SCK     → GPIO 18
//    MOSI    → GPIO 23
//    MISO    → GPIO 19
//    RST     → GPIO 22
//
//  Servo Motor (single shared gate):
//    Signal  → GPIO 13
//    VCC     → 5V (use external supply if servo jitters)
//    GND     → GND (common with ESP32)
//
//  LCD (I2C):
//    SDA → GPIO 4
//    SCL → GPIO 5
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
#define RFID_SS_PIN   21
#define RFID_RST_PIN  22
#define SERVO_PIN     13   // Single shared gate servo

// ── Objects ──────────────────────────────────────────────────
MFRC522           rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo             gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Uses GPIO 4 (SDA) and GPIO 5 (SCL)

// ── Gate Config ──────────────────────────────────────────────
const int GATE_OPEN    = 90;   // degrees — adjust if your servo differs
const int GATE_CLOSED  = 0;    // degrees
const int GATE_HOLD_MS = 5000; // gate stays open 5 seconds

// ── Global State ─────────────────────────────────────────────
int  permFree  = 4;
int  visitFree = 4;
int  lastAssignedSlot = 0;  // Track last assigned permanent slot
unsigned long cardDetectTime = 0;  // Track when card was last detected
String lastCardUID = "";  // Store last card's UID for exit detection
bool  isEntryMode = true;  // Track if we're waiting for entry or exit

// ── Timing ───────────────────────────────────────────────────
unsigned long lastFetch        = 0;
unsigned long lastScreenUpdate = 0;
unsigned long gateCloseTime    = 0;
bool          isGateOpen       = false;

// ── Forward Declarations ─────────────────────────────────────
void lcdPrint(String line1, String line2);
void showStatusScreen();
void openGate();
void closeGate();
void connectWiFi();
void reconnectWiFi();
void fetchSlotStatus();
void notifyBackendSlotOccupied(String uid);
void notifyBackendExit(String uid);

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
  // SPI.begin(SCK, MOSI, MISO, SS)
  Serial.println("\n[SPI] Initializing SPI bus...");
  SPI.begin(18, 23, 19, 21);
  delay(100);

  // Hardware RST pin reset
  Serial.println("[RFID] Performing hard reset on RST pin (GPIO 22)...");
  pinMode(RFID_RST_PIN, OUTPUT);
  digitalWrite(RFID_RST_PIN, LOW);
  delay(100);
  digitalWrite(RFID_RST_PIN, HIGH);
  delay(100);

  // Initialize MFRC522
  Serial.println("[RFID] Initializing MFRC522...");
  rfid.PCD_Init();
  delay(200);

  // Software reset
  Serial.println("[RFID] Software reset...");
  rfid.PCD_Reset();
  delay(100);

  // Check version multiple times
  byte version = 0;
  for (int attempt = 0; attempt < 3; attempt++) {
    version = rfid.PCD_ReadRegister(rfid.VersionReg);
    Serial.print("[Attempt ");
    Serial.print(attempt + 1);
    Serial.print("] RFID Version: 0x");
    Serial.println(version, HEX);
    if (version != 0x00 && version != 0xFF) {
      break;
    }
    delay(50);
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

    // Configure antenna
    Serial.println("[RFID] Configuring antenna gain...");
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    delay(50);

    // Self-test
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
void loop() {

  // ── Auto-close gate after hold time ──────────────────────
  if (isGateOpen && millis() >= gateCloseTime) {
    closeGate();
    lcdPrint("Gate Closed", "Drive safely!");
    lastScreenUpdate = millis() + 1500;
  }

  // ── RFID check — continuous polling ──────────────────────
  // IMPORTANT: Must NOT use early return() here — it breaks the loop!
  
  if (rfid.PICC_IsNewCardPresent()) {
    Serial.println("[RFID] Card detected! Processing...");
    
    // Check if this is the same card trying to exit
    if (!isEntryMode && lastCardUID != "") {
      Serial.println("[EXIT] Previous card detected again - releasing slot");
      Serial.print("Releasing with UID: ");
      Serial.println(lastCardUID);
      
      // Notify backend of exit
      notifyBackendExit(lastCardUID);
      
      // Close gate after exit
      lcdPrint("Exiting...", "Goodbye!");
      delay(1500);
      
      // Reset state
      isEntryMode = true;
      lastCardUID = "";
      lastAssignedSlot = 0;
      
      // Cleanup RFID state
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      delay(500);
      
    } else if (isEntryMode) {
      // ENTRY MODE - assign a parking slot
      if (permFree > 0) {
        // Calculate which permanent slot to assign (1-4)
        lastAssignedSlot = 5 - permFree;  // If permFree=4 → slot 1, permFree=3 → slot 2, etc.
        
        // Generate UID based on slot number (consistent for exit detection)
        lastCardUID = "PERM_SLOT_" + String(lastAssignedSlot);
        
        Serial.println("─────────────────────────");
        Serial.print("✓ Card assigned to Permanent Slot: ");
        Serial.println(lastAssignedSlot);
        
        // Notify backend of slot occupation
        notifyBackendSlotOccupied(lastCardUID);
        
        // Open gate for entry
        openGate();
        lcdPrint("Perm Slot " + String(lastAssignedSlot), "Welcome!");
        
        // Update slot count
        permFree--;
        Serial.print("Permanent slots remaining: ");
        Serial.println(permFree);
        
        // Set exit mode - wait for card to appear again
        isEntryMode = false;
        
        // Wait for card to move away
        delay(1500);
        
      } else {
        // No permanent slots - try visitor slots
        if (visitFree > 0) {
          int visitorSlot = 5 - visitFree;
          
          // Generate UID based on visitor slot number
          lastCardUID = "VISIT_SLOT_" + String(visitorSlot);
          
          Serial.println("─────────────────────────");
          Serial.print("✓ Card assigned to Visitor Slot: ");
          Serial.println(visitorSlot);
          
          // Notify backend of slot occupation
          notifyBackendSlotOccupied(lastCardUID);
          
          // Open gate for entry
          openGate();
          lcdPrint("Visit Slot " + String(visitorSlot), "Welcome!");
          
          // Update slot count
          visitFree--;
          Serial.print("Visitor slots remaining: ");
          Serial.println(visitFree);
          
          // Set exit mode
          isEntryMode = false;
          
          // Wait for card to move away
          delay(1500);
        } else {
          // All slots full
          Serial.println("✗ Parking lot FULL - all slots occupied");
          lcdPrint("FULL!", "Try later");
          lastScreenUpdate = millis() + 2000;
          delay(1000);
        }
      }
      
      // Cleanup RFID state
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

  // Send in backend-expected format: { uid: "..." }
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

  // Send same UID to trigger exit in backend
  String payload = "{\"uid\":\"" + uid + "\"}";
  
  Serial.print("→ EXIT: Sending to backend ");
  Serial.println(payload);
  
  int httpCode = http.POST(payload);

  Serial.print("Backend response: HTTP ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✓ Slot released: " + response);
    
    // Refetch slot status to update display
    fetchSlotStatus();
    lastFetch = millis();
  } else {
    Serial.print("⚠ Backend failed: HTTP ");
    Serial.println(httpCode);
  }

  http.end();
}

// ── GET /rfid → get result from server (DEPRECATED) ──────────
// REMOVED - Now using direct slot assignment

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