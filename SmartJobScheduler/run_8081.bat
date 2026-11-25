@echo off
setlocal
cd /d "%~dp0"
if not exist data mkdir data
if not exist SmartJobScheduler.exe (
  call compile.bat
  if errorlevel 1 exit /b 1
)
echo Starting Smart Job Scheduler on http://localhost:8081
echo Keep this window open. Press Ctrl+C to stop.
start "" /b cmd /c "timeout /t 2 /nobreak >nul & start http://localhost:8081"
SmartJobScheduler.exe 8081
endlocal
