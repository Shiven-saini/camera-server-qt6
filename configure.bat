@echo off
echo Configuring Camera Server Qt6...
echo.

REM Check if Qt6 is available
where qmake >nul 2>&1
if %errorlevel% neq 0 (
    echo Qt6 not found in PATH. Please run setup.bat first or install Qt6.
    pause
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build

REM Check if build directory has existing CMake cache with different generator
if exist "build\CMakeCache.txt" (
    echo Found existing CMake cache. Checking for generator conflicts...
    findstr /C:"CMAKE_GENERATOR" build\CMakeCache.txt >nul 2>&1
    if %errorlevel% equ 0 (
        echo.
        echo Existing CMake configuration found. Cleaning build directory...
        cd build
        del /Q /S * >nul 2>&1
        for /d %%p in (*) do rmdir "%%p" /s /q >nul 2>&1
        cd ..
        echo Build directory cleaned.
        echo.
    )
)

cd build

REM Try different generators based on what's available
echo Detecting available build tools...

REM First try Visual Studio 2022
where devenv >nul 2>&1
if %errorlevel% equ 0 (
    echo Found Visual Studio 2022, using VS generator...
    cmake .. -G "Visual Studio 17 2022" -A x64
    goto :check_result
)

REM Try Visual Studio 2019
where devenv >nul 2>&1
if %errorlevel% equ 0 (
    echo Found Visual Studio 2019, using VS generator...
    cmake .. -G "Visual Studio 16 2019" -A x64
    goto :check_result
)

REM Try Ninja (faster builds)
where ninja >nul 2>&1
if %errorlevel% equ 0 (
    echo Found Ninja, using Ninja generator...
    cmake .. -G "Ninja"
    goto :check_result
)

REM Fallback to default generator
echo Using default generator...
cmake ..

:check_result
if %errorlevel% neq 0 (
    echo.
    echo CMake configuration failed!
    echo.
    echo Possible solutions:
    echo 1. Make sure Qt6 is installed and in PATH
    echo 2. Set Qt6_DIR environment variable to Qt6 cmake directory
    echo 3. Example: set Qt6_DIR=C:\Qt\6.5.3\msvc2019_64\lib\cmake\Qt6
    echo 4. Install Visual Studio 2019/2022 or MinGW for compilation
    echo.
    echo Available generators:
    cmake --help
    echo.
    pause
    exit /b 1
)

echo.
echo Configuration completed successfully!
echo You can now run build.bat to build the project.
echo.
pause
