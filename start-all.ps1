# Smart Parking System - Start Both Services

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  Smart Parking System - Starting All Services" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Check if node_modules exists, if not install dependencies
if (-not (Test-Path "node_modules")) {
    Write-Host "Installing dependencies..." -ForegroundColor Yellow
    npm install
    Write-Host ""
}

# Start Node.js backend in a new window
Write-Host "Starting Backend (Node.js on port 3000)..." -ForegroundColor Green
$scriptPath = $PSScriptRoot
$cmd = "Set-Location '$scriptPath'; node server.js"
Start-Process powershell -ArgumentList "-NoExit", "-Command", $cmd

# Wait a moment for server to start
Start-Sleep -Seconds 2

# Open dashboard in default browser
$dashboardPath = Join-Path -Path $PSScriptRoot -ChildPath "dashboard.html"
Write-Host "Opening Frontend (dashboard.html)..." -ForegroundColor Green
Start-Process -FilePath $dashboardPath

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Services started!" -ForegroundColor Green
Write-Host "  Backend:  http://localhost:3000" -ForegroundColor Cyan
Write-Host "  Frontend: dashboard.html opened in browser" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To stop the backend, close the Node.js window" -ForegroundColor Yellow
