@echo off
echo Building Camera Server Qt6...

REM Check if build directory exists and is configured
if not exist "build" (
    echo Build directory not found. Please run configure.bat first.
    pause
    exit /b 1
)

if not exist "build\CMakeCache.txt" (
    echo CMake not configured. Please run configure.bat first.
    pause
    exit /b 1
)

cd build

REM Build the project
echo Building project...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    echo.
    echo Common issues:
    echo 1. Make sure Qt6 is installed and accessible
    echo 2. Run configure.bat to reconfigure if needed
    echo 3. Check that all required Qt6 modules are installed
    echo.
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable location: build\Release\CameraServerQt6.exe
echo.
echo To run the application:
echo   cd build\Release
echo   CameraServerQt6.exe
echo.
echo To install as Windows service:
echo   CameraServerQt6.exe (use Service menu in GUI)
echo.
pause
