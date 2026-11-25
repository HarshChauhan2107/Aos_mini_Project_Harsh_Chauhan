@echo off
setlocal
cd /d "%~dp0"
echo.
echo ================================================
echo  SMART JOB SCHEDULER - BUILD C BACKEND
echo ================================================
echo.
where gcc >nul 2>nul
if errorlevel 1 (
  echo ERROR: GCC was not found in PATH.
  echo.
  echo Install a MinGW-w64 GCC toolchain, for example MSYS2 UCRT64.
  echo Then open a NEW Command Prompt and confirm: gcc --version
  echo Read README.md for the setup instructions.
  pause
  exit /b 1
)
if not exist data mkdir data
gcc -std=c11 -O2 -Wall -Wextra src\server.c -o SmartJobScheduler.exe -lws2_32
if errorlevel 1 (
  echo.
  echo BUILD FAILED. Review the compiler messages above.
  pause
  exit /b 1
)
echo.
echo BUILD SUCCESSFUL: SmartJobScheduler.exe
echo.
endlocal
