#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QList>
#include "CameraConfig.h"

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();
    
    bool loadConfig();
    bool saveConfig();
    
    // Camera management
    void addCamera(const CameraConfig& camera);
    void updateCamera(const QString& id, const CameraConfig& camera);
    void removeCamera(const QString& id);
    QList<CameraConfig> getAllCameras() const;
    CameraConfig getCamera(const QString& id) const;
    
    // Settings
    bool isAutoStartEnabled() const { return m_autoStartEnabled; }
    void setAutoStartEnabled(bool enabled);
    
    int getNextExternalPort() const;
    
    // File paths
    QString getConfigFilePath() const;
    QString getLogFilePath() const;

signals:
    void configChanged();

private:
    ConfigManager();
    ~ConfigManager();
    
    void createDefaultConfig();
    void updateWindowsAutoStart();
    
    QList<CameraConfig> m_cameras;
    bool m_autoStartEnabled;
    QString m_configFilePath;
    QString m_logFilePath;
};

#endif // CONFIGMANAGER_H
