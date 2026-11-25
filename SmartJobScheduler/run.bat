@echo off
setlocal
cd /d "%~dp0"
if not exist data mkdir data
if not exist SmartJobScheduler.exe (
  call compile.bat
  if errorlevel 1 exit /b 1
)
echo.
echo ================================================
echo  SMART JOB SCHEDULER - STARTUP
echo ================================================
echo.
echo Starting the C backend on port 8080...
echo Your browser will open at http://localhost:8080
echo Keep this black Command Prompt window OPEN.
echo Press Ctrl+C in this window to stop the app.
echo.
start "" /b cmd /c "timeout /t 2 /nobreak >nul & start http://localhost:8080"
SmartJobScheduler.exe 8080
endlocal
