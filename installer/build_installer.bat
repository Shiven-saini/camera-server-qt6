@echo off
setlocal

echo ============================================
echo Visco Connect Demo WiX Installer Builder
echo ============================================

REM Set variables
set PROJECT_ROOT=%~dp0..
set INSTALLER_DIR=%~dp0
set BUILD_DIR=%PROJECT_ROOT%\build\bin
set OUTPUT_DIR=%INSTALLER_DIR%\output
set WIX_PATH=C:\Program Files (x86)\WiX Toolset v3.14\bin

REM Add WiX to PATH for this session
set PATH=%WIX_PATH%;%PATH%

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Check if WiX is installed
where heat.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: WiX Toolset not found in PATH!
    echo Please install WiX Toolset and add it to your PATH.
    pause
    exit /b 1
)

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory not found: %BUILD_DIR%
    echo Please build the application first using build.bat
    pause
    exit /b 1
)

echo Step 1: Creating icon file...
REM Check for ICO file
if exist "%INSTALLER_DIR%\camera_server_icon.ico" (
    echo Icon file found: camera_server_icon.ico
) else (
    echo WARNING: Icon file not found. Creating a default reference...
    REM Create a simple default icon reference
    copy NUL "%INSTALLER_DIR%\camera_server_icon.ico" >nul 2>&1
    echo Default icon placeholder created.
    echo You can replace it with a real ICO file for better appearance.
)

echo Step 2: Harvesting files from build directory...
REM Use Heat to harvest all files from the build\bin directory
heat.exe dir "%BUILD_DIR%" ^
    -cg HarvestedFiles ^
    -gg ^
    -scom ^
    -sreg ^
    -sfrag ^
    -srd ^
    -dr INSTALLFOLDER ^
    -var var.BuildDir ^
    -out "%INSTALLER_DIR%\HarvestedFiles.wxs"

if %errorlevel% neq 0 (
    echo ERROR: Failed to harvest files!
    pause
    exit /b 1
)

echo Step 3: Compiling WiX source files...
REM Compile the main WXS file and harvested files
candle.exe -arch x64 ^
    -dSourceDir="%PROJECT_ROOT%" ^
    -dBuildDir="%BUILD_DIR%" ^
    "%INSTALLER_DIR%\CameraServerBasic.wxs" ^
    "%INSTALLER_DIR%\HarvestedFiles.wxs" ^
    -out "%OUTPUT_DIR%\\"

if %errorlevel% neq 0 (
    echo ERROR: Failed to compile WiX source files!
    pause
    exit /b 1
)

echo Step 4: Linking MSI package...
REM Link the compiled objects into an MSI
light.exe -ext WixUIExtension ^
    "%OUTPUT_DIR%\CameraServerBasic.wixobj" ^
    "%OUTPUT_DIR%\HarvestedFiles.wixobj" ^
    -out "%OUTPUT_DIR%\ViscoConnectDemo_v2.1.5_Setup.msi"

if %errorlevel% neq 0 (
    echo ERROR: Failed to link MSI package!
    pause
    exit /b 1
)

echo.
echo ============================================
echo SUCCESS: Installer created successfully!
echo ============================================
echo.
echo Output: %OUTPUT_DIR%\ViscoConnectDemo_v2.1.5_Setup.msi
echo.
echo To test the installer:
echo 1. Run as Administrator: %OUTPUT_DIR%\ViscoConnectDemo_v2.1.5_Setup.msi
echo 2. Follow the installation wizard
echo 3. Check that firewall rules are added
echo 4. Verify desktop shortcut is created
echo.

REM Clean up intermediate files
del "%OUTPUT_DIR%\*.wixobj" 2>nul
del "%OUTPUT_DIR%\*.wixpdb" 2>nul

echo Build complete! Press any key to exit.
pause >nul
