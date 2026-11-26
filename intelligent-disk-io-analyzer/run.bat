@echo off
cd /d "%~dp0"
if not exist disk_analyzer.exe call build.bat
if not exist disk_analyzer.exe (
  echo ERROR: disk_analyzer.exe was not created. Fix the build error first.
  pause
  exit /b 1
)
echo.
echo DiskScope is starting...
echo Open http://127.0.0.1:8080 in Chrome or Edge.
echo Keep this window open while using the project.
echo Press Ctrl+C to stop the server.
echo.
disk_analyzer.exe 8080
if errorlevel 1 (
  echo.
  echo The server could not start. Port 8080 may already be in use.
  echo Try: disk_analyzer.exe 8090
  echo Then open http://127.0.0.1:8090
)
pause
