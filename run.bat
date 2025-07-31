@echo off
echo Visco Connect - Run Application
echo ===================================

if not exist "build\Release\ViscoConnect.exe" (
    echo Application not built yet.
    echo Please run the following scripts in order:
    echo   1. setup.bat
    echo   2. configure.bat  
    echo   3. build.bat
    echo.
    pause
    exit /b 1
)

echo Starting Visco Connect v2.1.5...
echo.

cd build\Release

echo Application location: %CD%\ViscoConnect.exe
echo.

echo Starting application...
start ViscoConnect.exe

echo.
echo Visco Connect has been launched!
echo.
echo The application should now appear or be minimized to the system tray.
echo.
echo Configuration files will be created in:
echo %LOCALAPPDATA%\ViscoConnect\
echo.
echo Log files will be saved to:
echo %LOCALAPPDATA%\ViscoConnect\visco-connect.log
echo.
pause
