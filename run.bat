@echo off
echo Camera Server Qt6 - Run Application
echo ===================================

if not exist "build\Release\CameraServerQt6.exe" (
    echo Application not built yet.
    echo Please run the following scripts in order:
    echo   1. setup.bat
    echo   2. configure.bat  
    echo   3. build.bat
    echo.
    pause
    exit /b 1
)

echo Starting Camera Server Qt6...
echo.

cd build\Release

echo Application location: %CD%\CameraServerQt6.exe
echo.

echo Starting application...
start CameraServerQt6.exe

echo.
echo Camera Server Qt6 has been launched!
echo.
echo The application should now appear or be minimized to the system tray.
echo.
echo Configuration files will be created in:
echo %LOCALAPPDATA%\CameraServer\
echo.
echo Log files will be saved to:
echo %LOCALAPPDATA%\CameraServer\camera-server.log
echo.
pause
