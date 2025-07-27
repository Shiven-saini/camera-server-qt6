#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QDir>
#include <QStandardPaths>
#include <windows.h>

#include "MainWindow.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "WindowsService.h"

// Forward declaration for WireGuard service function
extern "C" {
    typedef bool (*WireGuardTunnelServiceFunc)(const wchar_t* configFile);
}

int main(int argc, char *argv[])
{
    // Check for WireGuard service mode first (before creating QApplication)
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "/service") {
        // This is a WireGuard tunnel service call
        QString configPath = QString::fromLocal8Bit(argv[2]);
        
        // Load tunnel.dll and call WireGuardTunnelService
        HMODULE tunnelDll = LoadLibraryA("tunnel.dll");
        if (!tunnelDll) {
            return 1;  // Failed to load tunnel.dll
        }
        
        WireGuardTunnelServiceFunc tunnelServiceFunc = 
            (WireGuardTunnelServiceFunc)GetProcAddress(tunnelDll, "WireGuardTunnelService");
        
        if (!tunnelServiceFunc) {
            FreeLibrary(tunnelDll);
            return 1;  // Failed to get function
        }
        
        // Convert to wide string and call the function
        std::wstring wideConfigPath = configPath.toStdWString();
        bool result = tunnelServiceFunc(wideConfigPath.c_str());
        
        FreeLibrary(tunnelDll);
        return result ? 0 : 1;
    }
    
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("CameraServerQt6");
    app.setApplicationDisplayName("Camera Server Qt6");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CameraServer");
    app.setOrganizationDomain("cameraserver.local");
    
    // Check if running as service
    bool runAsService = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--service") {
            runAsService = true;
            break;
        }
    }
    
    // Initialize logger
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataPath);
    Logger::instance().setLogFile(appDataPath + "/camera-server.log");
    Logger::instance().setLogLevel(LogLevel::Info);
    
    LOG_INFO("=== Camera Server Qt6 Starting ===", "Main");
    LOG_INFO(QString("Version: %1").arg(app.applicationVersion()), "Main");
    LOG_INFO(QString("Run as service: %1").arg(runAsService ? "Yes" : "No"), "Main");
    
    // Load configuration
    if (!ConfigManager::instance().loadConfig()) {
        LOG_ERROR("Failed to load configuration", "Main");
        if (!runAsService) {
            QMessageBox::critical(nullptr, "Error", "Failed to load configuration file");
        }
        return 1;
    }
    
    if (runAsService) {
        // Running as Windows service
        LOG_INFO("Starting Windows service mode", "Main");
        
        if (!WindowsService::instance().startServiceMode()) {
            LOG_ERROR("Failed to start service mode", "Main");
            return 1;
        }
          // Service mode - create minimal application without UI
        // The service will handle camera management in the background
        
        // TODO: In a real implementation, you would create a service-only version
        // that doesn't require GUI components. For now, we'll just run the console app.
        
        LOG_INFO("Service mode started successfully", "Main");
        return app.exec();
    } else {
        // Running as regular application
        LOG_INFO("Starting GUI application", "Main");
        
        // Check if system tray is available
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            QMessageBox::critical(nullptr, "System Tray",
                                "System tray is not available on this system.");
            return 1;
        }
        
        // Create main window
        LOG_INFO("Creating main window...", "Main");
        MainWindow window;
        LOG_INFO("Main window created", "Main");
        
        // Show the main window initially
        window.show();
        LOG_INFO("Main window shown", "Main");
        
        // Handle application quit cleanup
        QObject::connect(&app, &QApplication::aboutToQuit, []() {
            LOG_INFO("=== Camera Server Qt6 Shutting Down ===", "Main");
        });
        
        LOG_INFO("GUI application initialized successfully", "Main");
        
        return app.exec();
    }
}
