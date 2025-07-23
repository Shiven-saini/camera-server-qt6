#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QHash>
#include "CameraConfig.h"
#include "PortForwarder.h"

class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();
    
    void initialize();
    void shutdown();
    
    // Camera operations
    bool addCamera(const CameraConfig& camera);
    bool updateCamera(const QString& id, const CameraConfig& camera);
    bool removeCamera(const QString& id);
    
    // Service control
    void startCamera(const QString& id);
    void stopCamera(const QString& id);
    void startAllCameras();
    void stopAllCameras();
    
    // Status
    bool isCameraRunning(const QString& id) const;
    QStringList getRunningCameras() const;
    QList<CameraConfig> getAllCameras() const;

signals:
    void cameraStarted(const QString& id);
    void cameraStopped(const QString& id);
    void cameraError(const QString& id, const QString& error);
    void configurationChanged();

private slots:
    void handleForwardingStarted(const QString& cameraId, int externalPort);
    void handleForwardingStopped(const QString& cameraId);
    void handleForwardingError(const QString& cameraId, const QString& error);
    void handleConnectionEstablished(const QString& cameraId, const QString& clientAddress);
    void handleConnectionClosed(const QString& cameraId, const QString& clientAddress);

private:
    void loadConfiguration();
    void saveConfiguration();
    
    PortForwarder* m_portForwarder;
    QHash<QString, CameraConfig> m_cameras;
    QHash<QString, bool> m_cameraStatus; // id -> running status
};

#endif // CAMERAMANAGER_H
