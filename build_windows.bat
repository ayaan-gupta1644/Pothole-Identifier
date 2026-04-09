@echo off
REM ═══════════════════════════════════════════════════════════
REM  build_windows.bat
REM  One-click build script for Pothole Severity Classifier
REM  Works with Visual Studio 2019 and 2022
REM ═══════════════════════════════════════════════════════════

setlocal EnableDelayedExpansion

echo.
echo  =========================================
echo   Pothole Detector -- Windows Build Script
echo  =========================================
echo.

REM ── STEP 1: Set your OpenCV path ────────────────────────────
REM  Change this if you installed OpenCV somewhere else.
REM  This should be the folder that contains "OpenCVConfig.cmake"
REM  Typical locations:
REM    C:\opencv\build
REM    C:\tools\opencv\build
REM    D:\opencv\build

set OpenCV_DIR=C:\opencv\build

REM  Uncomment and edit the line below if needed:
REM set OpenCV_DIR=D:\opencv\build

echo  [1/4] OpenCV path: %OpenCV_DIR%

if not exist "%OpenCV_DIR%\OpenCVConfig.cmake" (
    echo.
    echo  ERROR: OpenCVConfig.cmake not found at %OpenCV_DIR%
    echo.
    echo  Fix:
    echo    1. Download OpenCV from https://opencv.org/releases/
    echo    2. Run the .exe installer (extracts to C:\opencv by default)
    echo    3. Edit the OpenCV_DIR line in this .bat file to match your path
    echo.
    pause
    exit /b 1
)

REM ── STEP 2: Detect Visual Studio / CMake generator ──────────
echo  [2/4] Detecting Visual Studio version...

set VS_GENERATOR=
set VS_NAME=

REM Check for VS 2022 first, then 2019, then 2017
where cl.exe >nul 2>&1
if !errorlevel! == 0 (
    cl.exe 2>&1 | findstr /C:"19.3" >nul && (
        set VS_GENERATOR=Visual Studio 17 2022
        set VS_NAME=Visual Studio 2022
    )
    cl.exe 2>&1 | findstr /C:"19.2" >nul && (
        if "!VS_GENERATOR!"=="" (
            set VS_GENERATOR=Visual Studio 16 2019
            set VS_NAME=Visual Studio 2019
        )
    )
)

REM Fallback: use VS 2022 generator and let CMake figure it out
if "!VS_GENERATOR!"=="" (
    echo  Could not auto-detect VS version. Defaulting to VS 2022.
    echo  If you have VS 2019, edit VS_GENERATOR in this script.
    set VS_GENERATOR=Visual Studio 17 2022
    set VS_NAME=Visual Studio 2022 (assumed)
)

echo  Using: !VS_NAME!

REM ── STEP 3: Run CMake configure ─────────────────────────────
echo  [3/4] Configuring with CMake...

if not exist build mkdir build
cd build

cmake .. ^
    -G "!VS_GENERATOR!" ^
    -A x64 ^
    -DOpenCV_DIR="%OpenCV_DIR%"

if !errorlevel! neq 0 (
    echo.
    echo  ERROR: CMake configuration failed.
    echo  Check the output above for details.
    echo.
    cd ..
    pause
    exit /b 1
)

REM ── STEP 4: Build in Release mode ───────────────────────────
echo  [4/4] Building in Release mode...

cmake --build . --config Release --parallel

if !errorlevel! neq 0 (
    echo.
    echo  ERROR: Build failed. Check errors above.
    echo.
    cd ..
    pause
    exit /b 1
)

cd ..

REM ── Done ─────────────────────────────────────────────────────
echo.
echo  =========================================
echo   Build successful!
echo  =========================================
echo.
echo  Executable:  build\Release\pothole_detector.exe
echo.
echo  Run commands (from this folder):
echo    build\Release\pothole_detector.exe --video road.mp4
echo    build\Release\pothole_detector.exe --camera 0
echo    build\Release\pothole_detector.exe --image road.jpg
echo    build\Release\pothole_detector.exe --video road.mp4 --save out.mp4
echo.
echo  Keys during playback:
echo    H = heatmap   S = save CSV   R = reset   Q = quit
echo.
pause
