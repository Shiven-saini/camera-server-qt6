@echo off
REM ------------------------------------------------------------------
REM  Setup script for CameraServerQt6 with MinGW
REM  This script finds a MinGW-based Qt installation and configures
REM  the environment variables needed for CMake to build the project.
REM ------------------------------------------------------------------

echo Camera Server Qt6 Setup (for MinGW)
echo ===================================
echo.
echo This application requires a MinGW version of Qt 6.5.3 or later.
echo.
echo Checking for Qt for MinGW installation...

REM --- Search for a MinGW Qt installation ---
set "QT_FOUND=0"
REM Define a list of common directories for MinGW versions of Qt. Add your path here if needed.
set "QT_MINGW_DIRS=C:\Qt\6.8.0\mingw_64 C:\Qt\6.7.0\mingw_64 C:\Qt\6.6.0\mingw_64 C:\Qt\6.5.3\mingw_64 C:\Qt6"

for %%D in (%QT_MINGW_DIRS%) do (
    if exist "%%~D\bin\qmake.exe" (
        echo   [OK] Found Qt for MinGW at: "%%~D"
        set "QT_PATH=%%~D"
        set "QT_FOUND=1"
        goto :qt_found
    )
)

:qt_not_found
echo.
echo   [ERROR] Qt for MinGW was not found in common directories.
echo.
echo   Please ensure you have installed Qt correctly:
echo   1. Run the Qt Online Installer from: https://www.qt.io/download-qt-installer
echo   2. During installation, expand the Qt version (e.g., 6.5.3).
echo   3. **IMPORTANT: Select the "MinGW" component (e.g., MinGW 11.2.0 64-bit).**
echo.
echo   After installation, you can either:
echo   A) Re-run this setup script.
echo   B) Manually set the environment variables in your terminal like this:
echo      set "CMAKE_PREFIX_PATH=C:\Qt\6.5.3\mingw_64"
echo.
pause
exit /b 1

:qt_found
echo.
echo Setting up build environment for Qt...
set "PATH=%QT_PATH%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_PATH%"

echo   - Path set to: %QT_PATH%\bin
echo   - CMAKE_PREFIX_PATH set to: %CMAKE_PREFIX_PATH%
echo.
echo Environment setup complete!
echo.
echo You can now use the following scripts:
echo   - configure.bat  (to generate build files with CMake)
echo   - build.bat      (to compile the project)
echo.
pause
