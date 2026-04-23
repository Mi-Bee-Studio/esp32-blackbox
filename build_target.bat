@echo off
setlocal enabledelayedexpansion

set "IDF_TOOLS_PATH=C:\Espressif"
set "IDF_PATH=C:\Users\micke\esp\.espressif\v6.0\esp-idf"
set "ESP_IDF_VERSION=6.0.0"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf6.0_py3.13_env"
set "PROJECT_DIR=c:\Users\micke\Projects\embedded\esp32-blackbox"

if "%~1"=="" (
    echo Usage: %~nx0 ^<target^> [action] [port]
    echo.
    echo Targets:
    echo   esp32c3    Build for ESP32-C3 SuperMini
    echo   esp32c6    Build for Seeed XIAO ESP32C6
    echo.
    echo Actions:
    echo   build      Build only ^(default^)
    echo   flash      Build and flash
    echo   monitor    Build, flash and monitor
    echo   clean      Full clean and build
    echo.
    echo Port:
    echo   COM port for flash, e.g. COM3 ^(default: auto-detect^)
    echo.
    echo Examples:
    echo   %~nx0 esp32c6
    echo   %~nx0 esp32c6 flash COM5
    echo   %~nx0 esp32c3 monitor COM3
    echo   %~nx0 esp32c6 clean
    exit /b 1
)

set "TARGET=%~1"
set "ACTION=%~2"
set "PORT=%~3"

if "%ACTION%"=="" set "ACTION=build"

echo ========================================
echo  ESP32 Blackbox Build System
echo  Target: %TARGET%
echo  Action: %ACTION%
echo ========================================

call "%IDF_PATH%\export.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize ESP-IDF environment
    echo Please verify IDF_PATH=%IDF_PATH%
    exit /b 1
)

cd /d "%PROJECT_DIR%"

if "%ACTION%"=="clean" (
    echo [1/3] Full clean...
    if exist build rmdir /s /q build
    if exist sdkconfig del sdkconfig
    if exist sdkconfig.old del sdkconfig.old
    echo [2/3] Setting target to %TARGET%...
    idf.py set-target %TARGET%
    if errorlevel 1 (
        echo ERROR: set-target failed
        exit /b 1
    )
    echo [3/3] Building...
    idf.py build
    if errorlevel 1 (
        echo ERROR: Build failed
        exit /b 1
    )
    echo ========================================
    echo  BUILD SUCCESS
    echo ========================================
    exit /b 0
)

if not exist sdkconfig (
    echo [1/3] First build - setting target to %TARGET%...
    idf.py set-target %TARGET%
    if errorlevel 1 (
        echo ERROR: set-target failed
        exit /b 1
    )
) else (
    findstr /c:"CONFIG_IDF_TARGET=\"%TARGET%\"" sdkconfig >nul 2>&1
    if errorlevel 1 (
        echo [1/3] Target changed - reconfiguring to %TARGET%...
        idf.py set-target %TARGET%
        if errorlevel 1 (
            echo ERROR: set-target failed
            exit /b 1
        )
    )
)

echo [2/3] Building...
idf.py build
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

if "%ACTION%"=="flash" (
    echo [3/3] Flashing...
    if "%PORT%"=="" (
        idf.py flash
    ) else (
        idf.py -p %PORT% flash
    )
    if errorlevel 1 (
        echo ERROR: Flash failed
        exit /b 1
    )
)

if "%ACTION%"=="monitor" (
    echo [3/3] Flashing and monitoring...
    if "%PORT%"=="" (
        idf.py flash monitor
    ) else (
        idf.py -p %PORT% flash monitor
    )
)

if "%ACTION%"=="build" (
    echo ========================================
    echo  BUILD SUCCESS
    echo  Firmware: %PROJECT_DIR%\build\esp32-blackbox.bin
    echo ========================================
)

exit /b 0
