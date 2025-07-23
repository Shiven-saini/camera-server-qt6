#include "CameraManager.h"
#include "ConfigManager.h"
#include "Logger.h"

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , m_portForwarder(nullptr)
{
    m_portForwarder = new PortForwarder(this);
    
    // Connect port forwarder signals
    connect(m_portForwarder, &PortForwarder::forwardingStarted,
            this, &CameraManager::handleForwardingStarted);
    connect(m_portForwarder, &PortForwarder::forwardingStopped,
            this, &CameraManager::handleForwardingStopped);
    connect(m_portForwarder, &PortForwarder::forwardingError,
            this, &CameraManager::handleForwardingError);
    connect(m_portForwarder, &PortForwarder::connectionEstablished,
            this, &CameraManager::handleConnectionEstablished);
    connect(m_portForwarder, &PortForwarder::connectionClosed,
            this, &CameraManager::handleConnectionClosed);
}

CameraManager::~CameraManager()
{
    shutdown();
}

void CameraManager::initialize()
{
    loadConfiguration();
    
    // Auto-start enabled cameras
    for (const CameraConfig& camera : m_cameras.values()) {
        if (camera.isEnabled()) {
            startCamera(camera.id());
        }
    }
    
    LOG_INFO("Camera manager initialized", "CameraManager");
}

void CameraManager::shutdown()
{
    stopAllCameras();
    LOG_INFO("Camera manager shutdown", "CameraManager");
}

bool CameraManager::addCamera(const CameraConfig& camera)
{
    if (!camera.isValid()) {
        LOG_ERROR(QString("Cannot add invalid camera: %1").arg(camera.name()), "CameraManager");
        return false;
    }
    
    ConfigManager::instance().addCamera(camera);
    loadConfiguration();
    
    LOG_INFO(QString("Camera added: %1").arg(camera.name()), "CameraManager");
    emit configurationChanged();
    return true;
}

bool CameraManager::updateCamera(const QString& id, const CameraConfig& camera)
{
    if (!camera.isValid()) {
        LOG_ERROR(QString("Cannot update to invalid camera configuration: %1").arg(camera.name()), "CameraManager");
        return false;
    }
    
    // Stop camera if it's running
    bool wasRunning = isCameraRunning(id);
    if (wasRunning) {
        stopCamera(id);
    }
    
    ConfigManager::instance().updateCamera(id, camera);
    loadConfiguration();
    
    // Restart camera if it was running and still enabled
    if (wasRunning && camera.isEnabled()) {
        startCamera(id);
    }
    
    LOG_INFO(QString("Camera updated: %1").arg(camera.name()), "CameraManager");
    emit configurationChanged();
    return true;
}

bool CameraManager::removeCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_WARNING(QString("Cannot remove non-existent camera: %1").arg(id), "CameraManager");
        return false;
    }
    
    stopCamera(id);
    QString cameraName = m_cameras[id].name();
    
    ConfigManager::instance().removeCamera(id);
    loadConfiguration();
    
    LOG_INFO(QString("Camera removed: %1").arg(cameraName), "CameraManager");
    emit configurationChanged();
    return true;
}

void CameraManager::startCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_ERROR(QString("Cannot start non-existent camera: %1").arg(id), "CameraManager");
        return;
    }
    
    const CameraConfig& camera = m_cameras[id];
    if (!camera.isEnabled()) {
        LOG_WARNING(QString("Cannot start disabled camera: %1").arg(camera.name()), "CameraManager");
        return;
    }
    
    if (isCameraRunning(id)) {
        LOG_WARNING(QString("Camera already running: %1").arg(camera.name()), "CameraManager");
        return;
    }
    
    if (m_portForwarder->startForwarding(camera)) {
        m_cameraStatus[id] = true;
        LOG_INFO(QString("Camera started: %1").arg(camera.name()), "CameraManager");
        emit cameraStarted(id);
    } else {
        LOG_ERROR(QString("Failed to start camera: %1").arg(camera.name()), "CameraManager");
        emit cameraError(id, "Failed to start port forwarding");
    }
}

void CameraManager::stopCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_WARNING(QString("Cannot stop non-existent camera: %1").arg(id), "CameraManager");
        return;
    }
    
    if (!isCameraRunning(id)) {
        return; // Already stopped
    }
    
    m_portForwarder->stopForwarding(id);
    m_cameraStatus[id] = false;
    
    LOG_INFO(QString("Camera stopped: %1").arg(m_cameras[id].name()), "CameraManager");
    emit cameraStopped(id);
}

void CameraManager::startAllCameras()
{
    for (const CameraConfig& camera : m_cameras.values()) {
        if (camera.isEnabled()) {
            startCamera(camera.id());
        }
    }
    
    LOG_INFO("Started all enabled cameras", "CameraManager");
}

void CameraManager::stopAllCameras()
{
    for (const QString& id : m_cameras.keys()) {
        stopCamera(id);
    }
    
    LOG_INFO("Stopped all cameras", "CameraManager");
}

bool CameraManager::isCameraRunning(const QString& id) const
{
    return m_cameraStatus.value(id, false);
}

QStringList CameraManager::getRunningCameras() const
{
    QStringList running;
    for (auto it = m_cameraStatus.begin(); it != m_cameraStatus.end(); ++it) {
        if (it.value()) {
            running.append(it.key());
        }
    }
    return running;
}

QList<CameraConfig> CameraManager::getAllCameras() const
{
    return m_cameras.values();
}

void CameraManager::handleForwardingStarted(const QString& cameraId, int externalPort)
{
    m_cameraStatus[cameraId] = true;
    emit cameraStarted(cameraId);
}

void CameraManager::handleForwardingStopped(const QString& cameraId)
{
    m_cameraStatus[cameraId] = false;
    emit cameraStopped(cameraId);
}

void CameraManager::handleForwardingError(const QString& cameraId, const QString& error)
{
    m_cameraStatus[cameraId] = false;
    emit cameraError(cameraId, error);
}

void CameraManager::handleConnectionEstablished(const QString& cameraId, const QString& clientAddress)
{
    if (m_cameras.contains(cameraId)) {
        LOG_DEBUG(QString("Connection established to camera %1 from %2")
                  .arg(m_cameras[cameraId].name())
                  .arg(clientAddress), "CameraManager");
    }
}

void CameraManager::handleConnectionClosed(const QString& cameraId, const QString& clientAddress)
{
    if (m_cameras.contains(cameraId)) {
        LOG_DEBUG(QString("Connection closed to camera %1 from %2")
                  .arg(m_cameras[cameraId].name())
                  .arg(clientAddress), "CameraManager");
    }
}

void CameraManager::loadConfiguration()
{
    m_cameras.clear();
    m_cameraStatus.clear();
    
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    for (const CameraConfig& camera : cameras) {
        m_cameras[camera.id()] = camera;
        m_cameraStatus[camera.id()] = false;
    }
}

void CameraManager::saveConfiguration()
{
    // Configuration is automatically saved by ConfigManager
}
