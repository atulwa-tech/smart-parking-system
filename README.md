# Smart Parking System - Complete Setup Guide

## 📋 Overview

This is a complete smart parking management system consisting of:
- **Backend**: Node.js Express server (Port 3000)
- **Frontend**: Flutter Web dashboard (Port 5000)
- **Hardware**: ESP32 with RFID reader and servo motors

---

## 🚀 Quick Start

### Option 1: Run Everything in One Command
```bash
RUN_ALL.bat
```
This will:
1. Install backend dependencies
2. Install Flutter dependencies
3. Start the backend server
4. Start the Flutter web frontend

### Option 2: Run Backend Only
```bash
RUN_BACKEND_ONLY.bat
```

---

## 📦 Prerequisites

### For Backend
- **Node.js** (v14+) - [Download](https://nodejs.org/)
- **npm** (comes with Node.js)

### For Frontend
- **Flutter** SDK - [Download](https://flutter.dev/docs/get-started/install)
- A web browser (Chrome recommended)

### For Hardware
- **ESP32** microcontroller
- **MFRC522** RFID reader
- **4x Servo motors**
- **16x2 LCD with I2C backpack**
- **WiFi connection**

---

## ⚙️ Configuration

### Backend Configuration
The backend runs on **localhost:3000** by default. Change PORT in `backend/server.js` if needed.

**Default Registered RFID Cards:**
```javascript
'1A2B3C4D' → Permanent (Car 1)
'5E6F7A8B' → Permanent (Car 2)
'9C0D1E2F' → Visitor (Guest 1)
```

Add more cards in `backend/server.js` in the `registeredCards` object.

### ESP32 Configuration
Update the following in `smart_parking.ino`:

```cpp
// WiFi Credentials
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Backend Server IP (your PC's local IP address)
// Find your IP: Run 'ipconfig' in Command Prompt
const char* SERVER_IP   = "http://192.168.x.x:3000";  // ← Change this!
```

**Finding Your PC's IP Address:**
```cmd
ipconfig
```
Look for "IPv4 Address" under your network adapter (e.g., 192.168.x.x)

### Frontend Configuration
The frontend is set to connect to `http://localhost:3000`. No additional configuration needed if running locally.

---

## 🔌 API Endpoints

### GET /slots
Returns all parking slots and statistics
```json
{
  "slots": {
    "permanent": { "1": {...}, "2": {...} },
    "visitor": { "1": {...}, "2": {...} }
  },
  "stats": {
    "permanentFree": 2,
    "permanentOccupied": 2,
    "visitorFree": 3,
    "visitorOccupied": 1,
    "totalFree": 5
  }
}
```

### POST /rfid
Handle RFID card scan from ESP32
```json
Request: { "uid": "1A2B3C4D" }
Response: {
  "success": true,
  "action": "entry",
  "slot": 1,
  "bookingId": "uuid"
}
```

### POST /book
Book a visitor slot from dashboard
```json
Request: { "name": "John Doe" }
Response: {
  "success": true,
  "bookingId": "uuid",
  "slot": 1
}
```

### POST /release
Release a booked slot
```json
Request: { "bookingId": "uuid" }
Response: {
  "success": true,
  "slot": 1
}
```

---

## 📱 Frontend Features

- **Real-time slot status** (permanent & visitor)
- **Book slots** for visitors
- **Release slots** from dashboard
- **View occupancy statistics**
- **Dark theme UI** with amber accents

---

## 🎛️ Hardware Setup

### Pin Connections

**RFID Module (SPI):**
- SDA/SS  → GPIO 5
- SCK     → GPIO 18
- MOSI    → GPIO 23
- MISO    → GPIO 19
- RST     → GPIO 4

**Servo Motors:**
- Slot 1 Gate → GPIO 13
- Slot 2 Gate → GPIO 12
- Slot 3 Gate → GPIO 14
- Slot 4 Gate → GPIO 27

**LCD Display (I2C):**
- SDA → GPIO 21
- SCL → GPIO 22
- Address: 0x27 (or 0x3F)

---

## 🔧 Troubleshooting

### Backend won't start
```bash
# Clear old installations
cd backend
rm -r node_modules
npm install
npm start
```

### Flutter web won't run
```bash
# Enable web support
flutter channel stable
flutter config --enable-web

# Run on custom port
flutter run -d web --web-port=5000
```

### ESP32 can't connect to backend
- Check WiFi credentials
- Verify backend IP address matches your PC's IP
- Ensure ESP32 and PC are on the same network
- Check Serial Monitor for connection errors

### RFID not detected
- Verify pin connections
- Check MFRC522 library is installed
- Try scanning a different card

---

## 📝 Project Structure

```
smart-parking-system/
├── backend/
│   ├── package.json
│   └── server.js
├── main.dart              (Flutter app entry + models + API service)
├── dashboard.dart         (UI components)
├── pubspec.yaml          (Flutter config)
├── smart_parking.ino     (ESP32 code)
├── RUN_ALL.bat           (Run backend + frontend)
└── RUN_BACKEND_ONLY.bat  (Run backend only)
```

---

## 📊 System Architecture

```
┌─────────────────┐
│   ESP32 + RFID  │
│   (Hardware)    │
└────────┬────────┘
         │ POST /rfid
         ▼
┌─────────────────┐        ┌──────────────────┐
│  Node.js Backend│◄─────►│  Flutter Frontend │
│  (Port 3000)    │        │  (Port 5000)     │
└─────────────────┘        └──────────────────┘
     Serves APIs              Dashboard UI
  + Manages Data          + Books/Releases
  + Slot Logic            + Real-time Status
```

---

## 🎓 Adding New RFID Cards

1. Edit `backend/server.js`
2. Find the `registeredCards` object
3. Add new card:
```javascript
'YOUR_CARD_UID': { type: 'permanent', name: 'Car Name' },
```
4. Restart backend
5. Scan the card on ESP32

---

## 📞 Support

For issues or questions, check:
- Console logs in backend terminal
- Serial Monitor on Arduino IDE for ESP32 logs
- Flutter DevTools for frontend errors

---

**Version:** 1.0.0  
**Last Updated:** April 2026
