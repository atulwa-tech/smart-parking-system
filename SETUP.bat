@echo off
REM ============================================================
REM  Smart Parking System — Setup Wizard
REM ============================================================

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║    Smart Parking System - Setup Wizard                     ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

cd /d "%~dp0"

REM Check Node.js
echo [1/5] Checking Node.js installation...
where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ✗ Node.js not found! Please install from https://nodejs.org/
    pause
    exit /b 1
) else (
    for /f "tokens=*" %%i in ('node --version') do set NODE_VERSION=%%i
    echo ✓ Node.js !NODE_VERSION! found
)
echo.

REM Check Flutter
echo [2/5] Checking Flutter installation...
where flutter >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ✗ Flutter not found! Please install from https://flutter.dev/
    pause
    exit /b 1
) else (
    for /f "tokens=*" %%i in ('flutter --version') do set FLUTTER_INFO=%%i
    echo ✓ Flutter found
)
echo.

REM Install backend dependencies
echo [3/5] Installing backend dependencies...
cd backend
call npm install
if %ERRORLEVEL% NEQ 0 (
    echo ✗ Backend setup failed
    pause
    exit /b 1
)
cd ..
echo ✓ Backend dependencies installed
echo.

REM Install Flutter dependencies
echo [4/5] Installing Flutter dependencies...
call flutter pub get
if %ERRORLEVEL% NEQ 0 (
    echo ✗ Flutter setup failed
    pause
    exit /b 1
)
echo ✓ Flutter dependencies installed
echo.

REM Enable Flutter Web
echo [5/5] Enabling Flutter Web support...
call flutter config --enable-web >nul 2>nul
echo ✓ Flutter Web enabled
echo.

echo ╔════════════════════════════════════════════════════════════╗
echo ║  Setup Complete!                                           ║
echo ║                                                            ║
echo ║  Next Steps:                                              ║
echo ║  1. Edit backend/server.js to add your RFID cards         ║
echo ║  2. Update smart_parking.ino with your WiFi credentials   ║
echo ║  3. Run: RUN_ALL.bat to start the system                 ║
echo ║  4. Open frontend at http://localhost:5000               ║
echo ║                                                            ║
echo ║  See README.md for detailed configuration guide           ║
echo ╚════════════════════════════════════════════════════════════╝
echo.
pause
