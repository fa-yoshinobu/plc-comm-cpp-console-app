@echo off
setlocal

cd /d "%~dp0"
if errorlevel 1 (
    echo [ERROR] Could not enter the console-app repository root.
    exit /b 1
)

if "%PLATFORMIO_CORE_DIR%"=="" set "PLATFORMIO_CORE_DIR=%~d0\pio"
if not exist "%PLATFORMIO_CORE_DIR%" mkdir "%PLATFORMIO_CORE_DIR%"

set "PIO_EXE=pio"
where pio >nul 2>&1
if errorlevel 1 (
    if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" (
        set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
    ) else (
        echo [ERROR] PlatformIO was not found on PATH or in the standard user installation.
        exit /b 1
    )
)

set "PIO_VERSION="
for /f "tokens=4" %%V in ('"%PIO_EXE%" --version 2^>nul') do set "PIO_VERSION=%%V"
if not "%PIO_VERSION%"=="6.1.19" (
    echo [ERROR] PlatformIO Core 6.1.19 is required; found "%PIO_VERSION%".
    exit /b 1
)

echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo [1/4] Building Atom Matrix console...
"%PIO_EXE%" run -e m5stack-atom-console
if %errorlevel% neq 0 exit /b %errorlevel%

echo [2/4] Building W6300 console...
"%PIO_EXE%" run -e wiznet_6300_evb_pico2
if %errorlevel% neq 0 exit /b %errorlevel%

echo [3/4] Building T-RSS3 PLC verification console...
"%PIO_EXE%" run -e t-rss3-verification-console
if %errorlevel% neq 0 exit /b %errorlevel%

echo [4/4] Analyzing T-RSS3 owned sources...
"%PIO_EXE%" check -e t-rss3-verification-console --severity medium --severity high --fail-on-defect medium --fail-on-defect high
if %errorlevel% neq 0 exit /b %errorlevel%

echo [SUCCESS] Console app build checks passed.
endlocal
