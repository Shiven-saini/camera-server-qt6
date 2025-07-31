#include "ConfigManager.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ConfigManager::ConfigManager()
    : m_autoStartEnabled(false)
    , m_echoServerEnabled(true)
    , m_echoServerPort(7777)
{
    // Set up file paths
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataPath);
    
    m_configFilePath = appDataPath + "/config.json";
    m_logFilePath = appDataPath + "/visco-connect.log";
}

ConfigManager::~ConfigManager()
{
    saveConfig();
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig()
{
    QFile file(m_configFilePath);
    if (!file.exists()) {
        LOG_INFO("Config file does not exist, creating default configuration", "Config");
        createDefaultConfig();
        return saveConfig();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Failed to open config file: %1").arg(file.errorString()), "Config");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse config file: %1").arg(parseError.errorString()), "Config");
        return false;
    }
    
    QJsonObject root = doc.object();
      // Load settings
    m_autoStartEnabled = root["autoStart"].toBool(false);
    m_echoServerEnabled = root["echoServerEnabled"].toBool(true);
    m_echoServerPort = root["echoServerPort"].toInt(7777);
    
    // Load cameras
    m_cameras.clear();
    QJsonArray camerasArray = root["cameras"].toArray();
    for (const QJsonValue& value : camerasArray) {
        CameraConfig camera;
        camera.fromJson(value.toObject());
        m_cameras.append(camera);
    }
    
    LOG_INFO(QString("Loaded configuration with %1 cameras").arg(m_cameras.size()), "Config");
    return true;
}

bool ConfigManager::saveConfig()
{
    QJsonObject root;
      // Save settings
    root["autoStart"] = m_autoStartEnabled;
    root["echoServerEnabled"] = m_echoServerEnabled;
    root["echoServerPort"] = m_echoServerPort;
    
    // Save cameras
    QJsonArray camerasArray;
    for (const CameraConfig& camera : m_cameras) {
        camerasArray.append(camera.toJson());
    }
    root["cameras"] = camerasArray;
    
    QJsonDocument doc(root);
    
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to save config file: %1").arg(file.errorString()), "Config");
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO("Configuration saved successfully", "Config");
    emit configChanged();
    return true;
}

void ConfigManager::addCamera(const CameraConfig& camera)
{
    CameraConfig newCamera = camera;
    newCamera.setExternalPort(getNextExternalPort());
    m_cameras.append(newCamera);
    saveConfig();
    
    LOG_INFO(QString("Added camera: %1 (%2:%3 -> %4)")
             .arg(camera.name())
             .arg(camera.ipAddress())
             .arg(camera.port())
             .arg(newCamera.externalPort()), "Config");
}

void ConfigManager::updateCamera(const QString& id, const CameraConfig& camera)
{
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i].id() == id) {
            CameraConfig updatedCamera = camera;
            // Preserve external port
            updatedCamera.setExternalPort(m_cameras[i].externalPort());
            m_cameras[i] = updatedCamera;
            saveConfig();
            
            LOG_INFO(QString("Updated camera: %1").arg(camera.name()), "Config");
            return;
        }
    }
    
    LOG_WARNING(QString("Camera not found for update: %1").arg(id), "Config");
}

void ConfigManager::removeCamera(const QString& id)
{
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i].id() == id) {
            QString cameraName = m_cameras[i].name();
            m_cameras.removeAt(i);
            saveConfig();
            
            LOG_INFO(QString("Removed camera: %1").arg(cameraName), "Config");
            return;
        }
    }
    
    LOG_WARNING(QString("Camera not found for removal: %1").arg(id), "Config");
}

QList<CameraConfig> ConfigManager::getAllCameras() const
{
    return m_cameras;
}

CameraConfig ConfigManager::getCamera(const QString& id) const
{
    for (const CameraConfig& camera : m_cameras) {
        if (camera.id() == id) {
            return camera;
        }
    }
    return CameraConfig(); // Return empty config if not found
}

void ConfigManager::setAutoStartEnabled(bool enabled)
{
    if (m_autoStartEnabled != enabled) {
        m_autoStartEnabled = enabled;
        updateWindowsAutoStart();
        saveConfig();
        
        LOG_INFO(QString("Auto-start %1").arg(enabled ? "enabled" : "disabled"), "Config");
    }
}

void ConfigManager::setEchoServerEnabled(bool enabled)
{
    if (m_echoServerEnabled != enabled) {
        m_echoServerEnabled = enabled;
        saveConfig();
        
        LOG_INFO(QString("Echo server %1").arg(enabled ? "enabled" : "disabled"), "Config");
        emit configChanged();
    }
}

void ConfigManager::setEchoServerPort(int port)
{
    if (port < 1 || port > 65535) {
        LOG_WARNING(QString("Invalid echo server port: %1").arg(port), "Config");
        return;
    }
    
    if (m_echoServerPort != port) {
        m_echoServerPort = port;
        saveConfig();
        
        LOG_INFO(QString("Echo server port changed to %1").arg(port), "Config");
        emit configChanged();
    }
}

int ConfigManager::getNextExternalPort() const
{
    int maxPort = 8550; // Start from 8551
    
    for (const CameraConfig& camera : m_cameras) {
        if (camera.externalPort() > maxPort) {
            maxPort = camera.externalPort();
        }
    }
    
    return maxPort + 1;
}

QString ConfigManager::getConfigFilePath() const
{
    return m_configFilePath;
}

QString ConfigManager::getLogFilePath() const
{
    return m_logFilePath;
}

void ConfigManager::createDefaultConfig()
{
    m_cameras.clear();
    m_autoStartEnabled = false;
    m_echoServerEnabled = true;
    m_echoServerPort = 7777;
    
    LOG_INFO("Created default configuration", "Config");
}

void ConfigManager::updateWindowsAutoStart()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
      if (m_autoStartEnabled) {
        QString appPath = QCoreApplication::applicationFilePath();
        settings.setValue("ViscoConnect", appPath);
        LOG_INFO("Added application to Windows startup", "Config");
    } else {
        settings.remove("ViscoConnect");
        LOG_INFO("Removed application from Windows startup", "Config");
    }
#endif
}
