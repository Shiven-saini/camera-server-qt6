#include "SystemTrayManager.h"
#include "MainWindow.h"
#include "CameraManager.h"
#include "Logger.h"
#include <QApplication>
#include <QMessageBox>

SystemTrayManager::SystemTrayManager(MainWindow* mainWindow, CameraManager* cameraManager, QObject *parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_mainWindow(mainWindow)
    , m_cameraManager(cameraManager)
{
}

SystemTrayManager::~SystemTrayManager()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

void SystemTrayManager::initialize()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG_WARNING("System tray is not available", "SystemTrayManager");
        return;
    }
    
    createTrayIcon();
    createContextMenu();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::handleTrayIconActivated);
    
    updateTrayIconToolTip();
    updateCameraStatus();
    
    // Show the tray icon immediately
    m_trayIcon->show();
    
    // Show initial notification
    showNotification("Camera Server Qt6", "Application started and running in system tray");
    
    LOG_INFO("System tray manager initialized", "SystemTrayManager");
}

void SystemTrayManager::show()
{
    if (m_trayIcon) {
        m_trayIcon->show();
    }
}

void SystemTrayManager::hide()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool SystemTrayManager::isVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void SystemTrayManager::handleTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
            handleShowMainWindow();
            break;
        case QSystemTrayIcon::Trigger:
            // Single click - do nothing for now
            break;
        case QSystemTrayIcon::MiddleClick:
            // Middle click - do nothing for now
            break;
        case QSystemTrayIcon::Context:
            // Context menu will be shown automatically
            break;
        default:
            break;
    }
}

void SystemTrayManager::handleShowMainWindow()
{
    emit showMainWindow();
}

void SystemTrayManager::handleEnableAllCameras()
{
    if (m_cameraManager) {
        m_cameraManager->startAllCameras();
        showNotification("Camera Server Qt6", "Starting all cameras...", QSystemTrayIcon::Information);
        LOG_INFO("All cameras enabled via system tray", "SystemTrayManager");
    }
}

void SystemTrayManager::handleDisableAllCameras()
{
    if (m_cameraManager) {
        m_cameraManager->stopAllCameras();
        showNotification("Camera Server Qt6", "Stopping all cameras...", QSystemTrayIcon::Information);
        LOG_INFO("All cameras disabled via system tray", "SystemTrayManager");
    }
}

void SystemTrayManager::handleQuitApplication()
{
    LOG_INFO("Quit requested via system tray", "SystemTrayManager");
    emit quitApplication();
}

void SystemTrayManager::updateCameraStatus()
{
    if (!m_cameraManager) {
        return;
    }
    
    QStringList runningCameras = m_cameraManager->getRunningCameras();
    int totalCameras = m_cameraManager->getAllCameras().size();
    
    // Update action states
    if (m_enableAllAction) {
        m_enableAllAction->setEnabled(runningCameras.size() < totalCameras);
    }
    
    if (m_disableAllAction) {
        m_disableAllAction->setEnabled(!runningCameras.isEmpty());
    }
    
    updateTrayIconToolTip();
}

void SystemTrayManager::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    
    // Try to load custom icon first, fallback to system icon
    QIcon icon(":/icons/camera_server_icon.svg");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(icon);
}

void SystemTrayManager::createContextMenu()
{
    m_contextMenu = new QMenu();
    
    // Show main window action
    m_showAction = new QAction("Show Main Window", this);
    connect(m_showAction, &QAction::triggered, this, &SystemTrayManager::handleShowMainWindow);
    m_contextMenu->addAction(m_showAction);
    
    m_contextMenu->addSeparator();
    
    // Start all cameras action
    m_enableAllAction = new QAction("Start All Cameras", this);
    connect(m_enableAllAction, &QAction::triggered, this, &SystemTrayManager::handleEnableAllCameras);
    m_contextMenu->addAction(m_enableAllAction);
    
    // Stop all cameras action
    m_disableAllAction = new QAction("Stop All Cameras", this);
    connect(m_disableAllAction, &QAction::triggered, this, &SystemTrayManager::handleDisableAllCameras);
    m_contextMenu->addAction(m_disableAllAction);
    
    m_contextMenu->addSeparator();
    
    // Exit action
    m_quitAction = new QAction("Exit", this);
    connect(m_quitAction, &QAction::triggered, this, &SystemTrayManager::handleQuitApplication);
    m_contextMenu->addAction(m_quitAction);
    
    m_trayIcon->setContextMenu(m_contextMenu);
}

void SystemTrayManager::updateTrayIconToolTip()
{
    if (!m_trayIcon || !m_cameraManager) {
        return;
    }
    
    QStringList runningCameras = m_cameraManager->getRunningCameras();
    int totalCameras = m_cameraManager->getAllCameras().size();
    
    QString toolTip = QString("Camera Server Qt6\n%1 of %2 cameras running")
                      .arg(runningCameras.size())
                      .arg(totalCameras);
    
    m_trayIcon->setToolTip(toolTip);
}

void SystemTrayManager::showNotification(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, icon, 3000); // Show for 3 seconds
    }
}

void SystemTrayManager::notifyCameraStatusChange(const QString& cameraName, bool started)
{
    QString message = started ? 
        QString("Camera '%1' started successfully").arg(cameraName) :
        QString("Camera '%1' stopped").arg(cameraName);
    
    QSystemTrayIcon::MessageIcon icon = started ? 
        QSystemTrayIcon::Information : 
        QSystemTrayIcon::Warning;
    
    showNotification("Camera Server Qt6", message, icon);
}
