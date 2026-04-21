@echo off
REM ============================================================
REM  Smart Parking System — Start Backend Only
REM ============================================================

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║      Smart Parking Backend - Starting...                   ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

cd /d "%~dp0"

REM Check if backend dependencies are installed
if not exist "backend\node_modules" (
    echo Installing backend dependencies...
    cd backend
    call npm install
    cd ..
    echo.
)

REM Start backend server
echo Backend Server starting on http://localhost:3000...
echo.
cd backend
npm start

pause
