@echo off
REM ============================================================
REM  Smart Parking System — Run Frontend + Backend
REM ============================================================

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║      Smart Parking System - Startup Script                 ║
echo ║      Running Backend + Frontend (Flutter Web)              ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

REM Change to project directory
cd /d "%~dp0"

REM Check if backend dependencies are installed
if not exist "backend\node_modules" (
    echo [1/4] Installing backend dependencies...
    cd backend
    call npm install
    cd ..
    echo.
) else (
    echo [1/4] Backend dependencies already installed (skipping)
    echo.
)

REM Check if Flutter project dependencies are installed
if not exist "pubspec.lock" (
    echo [2/4] Installing Flutter dependencies...
    call flutter pub get
    echo.
) else (
    echo [2/4] Flutter dependencies already installed (skipping)
    echo.
)

REM Start backend server in new window
echo [3/4] Starting Backend Server on http://localhost:3000...
echo.
start "Smart Parking Backend" cmd /k "cd backend && npm start"

REM Wait a moment for backend to start
timeout /t 2 /nobreak

REM Start Flutter web frontend in new window
echo [4/4] Starting Flutter Web Frontend on http://localhost:5000...
echo.
start "Smart Parking Frontend" cmd /k "flutter run -d web --web-port=5000"

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║  Backend:   http://localhost:3000                          ║
echo ║  Frontend:  http://localhost:5000                          ║
echo ║  ESP32:     Configure SERVER_IP to your PC's local IP      ║
echo ║             (e.g., http://192.168.x.x:3000)               ║
echo ╚════════════════════════════════════════════════════════════╝
echo.
pause
