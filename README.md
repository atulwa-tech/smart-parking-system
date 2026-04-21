# 🅿 Smart Parking System

A complete IoT parking management system with RFID permanent slots, web-based visitor booking, and a live dashboard.

---

## 📁 Files

| File | Description |
|------|-------------|
| `server.js` | Node.js + Express backend |
| `dashboard.html` | Web dashboard (open in browser) |
| `smart_parking.ino` | ESP32 Arduino firmware |

---

## ⚡ Quick Start

### 1. Backend (Node.js)
```bash
npm install express cors body-parser
node server.js
# Runs on http://localhost:3000
```

### 2. Frontend
Open `dashboard.html` in any browser.
Edit `const API = 'http://localhost:3000'` to match your server IP.

### 3. ESP32
1. Install libraries in Arduino IDE:
   - MFRC522
   - ESP32Servo
   - LiquidCrystal_I2C
   - ArduinoJson

2. Edit in `smart_parking.ino`:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char* SERVER_IP     = "http://192.168.x.x:3000"; // your PC's IP
   ```

3. Flash to ESP32 (Board: "ESP32 Dev Module")

---

## 🔌 Pin Wiring (ESP32)

### MFRC522 RFID
| RFID Pin | ESP32 GPIO |
|----------|-----------|
| SDA/SS   | 5         |
| SCK      | 18        |
| MOSI     | 23        |
| MISO     | 19        |
| RST      | 4         |
| GND      | GND       |
| 3.3V     | 3.3V      |

### Servo Motors
| Slot | GPIO |
|------|------|
| 1    | 13   |
| 2    | 12   |
| 3    | 14   |
| 4    | 27   |

### LCD (I2C)
| Pin | ESP32 |
|-----|-------|
| SDA | 21    |
| SCL | 22    |
| VCC | 5V    |
| GND | GND   |

---

## 📡 API Reference

| Method | Endpoint | Body | Description |
|--------|----------|------|-------------|
| GET | `/slots` | — | Get all slot status + stats |
| POST | `/rfid` | `{"uid":"A1B2C3D4"}` | RFID check, returns slot |
| POST | `/book` | `{"name":"John"}` | Book visitor slot |
| POST | `/release` | `{"bookingId":"BK-xxx"}` | Release visitor slot |

---

## 🔑 Adding RFID Cards

Edit `rfidRegistry` in `server.js`:
```js
const rfidRegistry = {
  "A1B2C3D4": 1,  // Card UID → Slot number
  "E5F6G7H8": 2,
  "YOURNEWID": 3,
};
```
To find a card's UID, scan it and check ESP32 Serial Monitor output.

---

## 🏗 Architecture

```
RFID Card
    ↓ scan
ESP32 ──── WiFi ──→ Node.js Backend ←── Web Dashboard
  ↓                      ↓
Servo                JSON State
  ↓
LCD Display
```
