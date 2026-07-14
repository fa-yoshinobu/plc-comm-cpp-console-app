@echo off
setlocal

cd /d "%~dp0"
if errorlevel 1 (
    echo [ERROR] Could not enter the console-app repository root.
    exit /b 1
)

if "%PLATFORMIO_CORE_DIR%"=="" set "PLATFORMIO_CORE_DIR=%~d0\pio"
if not exist "%PLATFORMIO_CORE_DIR%" mkdir "%PLATFORMIO_CORE_DIR%"

set "TARGET=%~1"
if /I "%TARGET%"=="" set "TARGET=all"

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

if /I "%TARGET%"=="all" goto build_all
if /I "%TARGET%"=="atom" goto build_atom
if /I "%TARGET%"=="w6300" goto build_w6300
if /I "%TARGET%"=="trss3" goto build_trss3
if /I "%TARGET%"=="trss3-local" goto build_trss3_local
if /I "%TARGET%"=="stamplc" goto build_stamplc
if /I "%TARGET%"=="stamplc-local" goto build_stamplc_local

echo Usage: build_console.bat [all^|atom^|w6300^|trss3^|trss3-local^|stamplc^|stamplc-local]
exit /b 1

:build_all
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

echo [4/4] Building M5Stack StamPLC verification console...
"%PIO_EXE%" run -e stamplc-verification-console
if %errorlevel% neq 0 exit /b %errorlevel%

echo [SUCCESS] All console targets built successfully.
exit /b 0

:build_atom
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building Atom Matrix console...
"%PIO_EXE%" run -e m5stack-atom-console
exit /b %errorlevel%

:build_w6300
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building W6300 console...
"%PIO_EXE%" run -e wiznet_6300_evb_pico2
exit /b %errorlevel%

:build_trss3
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building T-RSS3 console with registry libraries...
"%PIO_EXE%" run -e t-rss3-verification-console
exit /b %errorlevel%

:build_trss3_local
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building T-RSS3 console with sibling library worktrees...
"%PIO_EXE%" run -e t-rss3-verification-console-local
exit /b %errorlevel%

:build_stamplc
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building M5Stack StamPLC console with registry libraries...
"%PIO_EXE%" run -e stamplc-verification-console
exit /b %errorlevel%

:build_stamplc_local
echo Using PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
echo Building M5Stack StamPLC console with sibling library worktrees...
"%PIO_EXE%" run -e stamplc-verification-console-local
exit /b %errorlevel%
