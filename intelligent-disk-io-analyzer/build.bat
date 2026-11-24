@echo off
cd /d "%~dp0"
echo ==============================================
echo   DiskScope - Building C backend
echo ==============================================
where gcc >nul 2>nul
if errorlevel 1 (
  echo ERROR: GCC was not found.
  echo Install MinGW-w64 GCC, then add its bin folder to PATH.
  echo Close and reopen Command Prompt after installation.
  pause
  exit /b 1
)
gcc -std=c11 -O2 -Wall -Wextra -o disk_analyzer.exe src\server.c -lws2_32
if errorlevel 1 (
  echo.
  echo ERROR: Compilation failed. Read the compiler messages above.
  pause
  exit /b 1
)
echo.
echo SUCCESS: disk_analyzer.exe has been created.
echo Now double-click run.bat.
pause
