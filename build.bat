@echo off
echo Building PhobosLT for ESP32C3...
echo.

REM Пошук PlatformIO
set PIO_PATH=""
if exist "C:\Users\%USERNAME%\.platformio\penv\Scripts\platformio.exe" (
    set PIO_PATH="C:\Users\%USERNAME%\.platformio\penv\Scripts\platformio.exe"
) else if exist "C:\Users\%USERNAME%\.platformio\penv\Scripts\pio.exe" (
    set PIO_PATH="C:\Users\%USERNAME%\.platformio\penv\Scripts\pio.exe"
) else (
    echo PlatformIO not found in standard location!
    echo Please install PlatformIO first.
    pause
    exit /b 1
)

echo Found PlatformIO: %PIO_PATH%
echo.

echo Building project...
%PIO_PATH% run -e ESP32C3

if errorlevel 1 (
    echo.
    echo Build FAILED!
    pause
    exit /b 1
)

echo.
echo Build SUCCESS!
echo.
echo Firmware files created:
dir .pio\build\ESP32C3\firmware.* /TC
echo.
echo To upload to ESP32C3:
echo 1. Connect ESP32C3 via USB
echo 2. Run: build_and_upload.bat
echo.
pause