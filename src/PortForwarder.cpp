#include "PortForwarder.h"
#include "Logger.h"
#include "NetworkInterfaceManager.h"
#include <QNetworkProxy>
#include <QTimer>
#include <QNetworkInterface>

PortForwarder::PortForwarder(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
{
}

PortForwarder::~PortForwarder()
{
    stopAllForwarding();
}

bool PortForwarder::startForwarding(const CameraConfig& camera)
{
    if (!camera.isValid() || !camera.isEnabled()) {
        LOG_ERROR(QString("Invalid or disabled camera configuration: %1").arg(camera.name()), "PortForwarder");
        return false;
    }
    
    QString cameraId = camera.id();
    
    // Stop existing forwarding for this camera
    if (m_sessions.contains(cameraId)) {
        stopForwarding(cameraId);
    }
    
    // Create new session
    ForwardingSession* session = new ForwardingSession;
    session->camera = camera;
    session->server = new QTcpServer(this);
    session->isReconnecting = false;
    
    // Set up reconnect timer
    session->reconnectTimer = new QTimer(this);
    session->reconnectTimer->setSingleShot(true);
    session->reconnectTimer->setInterval(5000); // 5 seconds
    connect(session->reconnectTimer, &QTimer::timeout, this, &PortForwarder::handleReconnectTimer);
      // Connect server signals
    connect(session->server, &QTcpServer::newConnection, this, &PortForwarder::handleNewConnection);
    
    // Start listening on all interfaces
    if (!bindToAllInterfaces(session->server, camera.externalPort())) {
        LOG_ERROR(QString("Failed to start listening on port %1: %2")
                  .arg(camera.externalPort())
                  .arg(session->server->errorString()), "PortForwarder");
        delete session->server;
        delete session->reconnectTimer;
        delete session;
        return false;
    }
    
    m_sessions[cameraId] = session;
    
    LOG_INFO(QString("Started port forwarding for camera %1: %2:%3 -> 0.0.0.0:%4")
             .arg(camera.name())
             .arg(camera.ipAddress())
             .arg(camera.port())
             .arg(camera.externalPort()), "PortForwarder");
    
    emit forwardingStarted(cameraId, camera.externalPort());
    return true;
}

void PortForwarder::stopForwarding(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) {
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    
    // Close all connections
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        QTcpSocket* client = it.key();
        QTcpSocket* target = it.value();
        
        if (client) {
            client->disconnectFromHost();
            client->deleteLater();
        }
        if (target) {
            target->disconnectFromHost();
            target->deleteLater();
        }
    }
    session->connections.clear();
    
    // Stop server
    if (session->server) {
        session->server->close();
        session->server->deleteLater();
    }
    
    // Stop reconnect timer
    if (session->reconnectTimer) {
        session->reconnectTimer->stop();
        session->reconnectTimer->deleteLater();
    }
    
    delete session;
    m_sessions.remove(cameraId);
    
    LOG_INFO(QString("Stopped port forwarding for camera: %1").arg(cameraId), "PortForwarder");
    emit forwardingStopped(cameraId);
}

void PortForwarder::stopAllForwarding()
{
    QStringList cameraIds = m_sessions.keys();
    for (const QString& cameraId : cameraIds) {
        stopForwarding(cameraId);
    }
}

bool PortForwarder::isForwarding(const QString& cameraId) const
{
    return m_sessions.contains(cameraId);
}

QStringList PortForwarder::getActiveForwards() const
{
    return m_sessions.keys();
}

void PortForwarder::handleNewConnection()
{
    QTcpServer* server = qobject_cast<QTcpServer*>(sender());
    if (!server) return;
    
    // Find which camera this server belongs to
    QString cameraId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->server == server) {
            cameraId = it.key();
            break;
        }
    }
    
    if (cameraId.isEmpty()) {
        LOG_ERROR("Received connection for unknown server", "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    QTcpSocket* clientSocket = server->nextPendingConnection();
    
    if (!clientSocket) {
        return;
    }
    
    // Create connection to target camera
    QTcpSocket* targetSocket = new QTcpSocket(this);
    
    // Store socket mapping
    session->connections[clientSocket] = targetSocket;
    m_socketToCameraMap[clientSocket] = cameraId;
    m_socketToCameraMap[targetSocket] = cameraId;
    
    // Connect client socket signals
    connect(clientSocket, &QTcpSocket::disconnected, this, &PortForwarder::handleClientDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &PortForwarder::handleClientDataReady);
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &PortForwarder::handleConnectionError);
    
    // Connect target socket signals
    connect(targetSocket, &QTcpSocket::connected, this, &PortForwarder::handleTargetConnected);
    connect(targetSocket, &QTcpSocket::disconnected, this, &PortForwarder::handleTargetDisconnected);
    connect(targetSocket, &QTcpSocket::readyRead, this, &PortForwarder::handleTargetDataReady);
    connect(targetSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &PortForwarder::handleConnectionError);
    
    // Connect to target camera
    targetSocket->connectToHost(session->camera.ipAddress(), session->camera.port());
    
    QString clientAddress = QString("%1:%2").arg(clientSocket->peerAddress().toString()).arg(clientSocket->peerPort());
    LOG_INFO(QString("New connection from %1 for camera %2").arg(clientAddress).arg(session->camera.name()), "PortForwarder");
    
    emit connectionEstablished(cameraId, clientAddress);
}

void PortForwarder::handleClientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(clientSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        clientSocket->deleteLater();
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    QTcpSocket* targetSocket = session->connections.value(clientSocket);
    
    if (targetSocket) {
        targetSocket->disconnectFromHost();
        targetSocket->deleteLater();
        m_socketToCameraMap.remove(targetSocket);
    }
    
    session->connections.remove(clientSocket);
    m_socketToCameraMap.remove(clientSocket);
    
    QString clientAddress = QString("%1:%2").arg(clientSocket->peerAddress().toString()).arg(clientSocket->peerPort());
    LOG_DEBUG(QString("Client disconnected: %1").arg(clientAddress), "PortForwarder");
    
    emit connectionClosed(cameraId, clientAddress);
    clientSocket->deleteLater();
}

void PortForwarder::handleClientDataReady()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(clientSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    QTcpSocket* targetSocket = session->connections.value(clientSocket);
    
    if (targetSocket && targetSocket->state() == QAbstractSocket::ConnectedState) {
        forwardData(clientSocket, targetSocket);
    }
}

void PortForwarder::handleTargetConnected()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    LOG_DEBUG(QString("Connected to target camera: %1").arg(cameraId), "PortForwarder");
}

void PortForwarder::handleTargetDisconnected()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        targetSocket->deleteLater();
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    
    // Find and disconnect corresponding client
    QTcpSocket* clientSocket = nullptr;
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        if (it.value() == targetSocket) {
            clientSocket = it.key();
            break;
        }
    }
    
    if (clientSocket) {
        session->connections.remove(clientSocket);
        m_socketToCameraMap.remove(clientSocket);
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }
    
    m_socketToCameraMap.remove(targetSocket);
    targetSocket->deleteLater();
    
    // Setup reconnect if camera is still enabled
    if (session->camera.isEnabled() && !session->isReconnecting) {
        setupReconnectTimer(cameraId);
    }
}

void PortForwarder::handleTargetDataReady()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    
    // Find corresponding client socket
    QTcpSocket* clientSocket = nullptr;
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        if (it.value() == targetSocket) {
            clientSocket = it.key();
            break;
        }
    }
    
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        forwardData(targetSocket, clientSocket);
    }
}

void PortForwarder::handleConnectionError(QAbstractSocket::SocketError error)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QString cameraId = m_socketToCameraMap.value(socket);
    if (cameraId.isEmpty()) return;
    
    QString errorString = socket->errorString();
    LOG_WARNING(QString("Connection error for camera %1: %2").arg(cameraId).arg(errorString), "PortForwarder");
    
    emit forwardingError(cameraId, errorString);
}

void PortForwarder::handleReconnectTimer()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // Find which camera this timer belongs to
    QString cameraId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->reconnectTimer == timer) {
            cameraId = it.key();
            break;
        }
    }
    
    if (cameraId.isEmpty()) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    session->isReconnecting = false;
    
    LOG_INFO(QString("Reconnect timer expired for camera: %1").arg(session->camera.name()), "PortForwarder");
}

void PortForwarder::setupReconnectTimer(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    if (session->isReconnecting) return;
    
    session->isReconnecting = true;
    session->reconnectTimer->start();
    
    LOG_INFO(QString("Setup reconnect timer for camera: %1").arg(session->camera.name()), "PortForwarder");
}

void PortForwarder::forwardData(QTcpSocket* from, QTcpSocket* to)
{
    if (!from || !to) return;
    
    QByteArray data = from->readAll();
    if (!data.isEmpty()) {
        to->write(data);
    }
}

void PortForwarder::setNetworkInterfaceManager(NetworkInterfaceManager* manager)
{
    if (m_networkManager) {
        disconnect(m_networkManager, nullptr, this, nullptr);
    }
    
    m_networkManager = manager;
    
    if (m_networkManager) {
        connect(m_networkManager, &NetworkInterfaceManager::interfacesChanged,
                this, &PortForwarder::onNetworkInterfacesChanged);
        connect(m_networkManager, &NetworkInterfaceManager::wireGuardInterfaceStateChanged,
                this, &PortForwarder::onWireGuardStateChanged);
    }
}

NetworkInterfaceManager* PortForwarder::networkInterfaceManager() const
{
    return m_networkManager;
}

bool PortForwarder::bindToAllInterfaces(QTcpServer* server, quint16 port)
{
    // First try the standard approach - bind to all interfaces
    if (server->listen(QHostAddress::Any, port)) {
        LOG_INFO(QString("Successfully bound to all interfaces (0.0.0.0:%1)").arg(port), "PortForwarder");
        return true;
    }
    
    LOG_WARNING(QString("Failed to bind to 0.0.0.0:%1, trying specific interfaces").arg(port), "PortForwarder");
    
    // If we have a network manager, try to bind to specific interfaces
    if (m_networkManager) {
        const auto activeInterfaces = m_networkManager->getActiveInterfaces();
        const auto addresses = m_networkManager->getAllAddresses();
        
        // Try to bind to each active interface address
        for (const QHostAddress& address : addresses) {
            if (server->listen(address, port)) {
                LOG_INFO(QString("Successfully bound to specific interface (%1:%2)")
                         .arg(address.toString()).arg(port), "PortForwarder");
                return true;
            }
        }
        
        // Try WireGuard interface specifically
        const QHostAddress wgAddress = m_networkManager->getWireGuardAddress();
        if (!wgAddress.isNull() && server->listen(wgAddress, port)) {
            LOG_INFO(QString("Successfully bound to WireGuard interface (%1:%2)")
                     .arg(wgAddress.toString()).arg(port), "PortForwarder");
            return true;
        }
    }
    
    // Last resort - try localhost
    if (server->listen(QHostAddress::LocalHost, port)) {
        LOG_WARNING(QString("Only bound to localhost (127.0.0.1:%1) - external access limited").arg(port), "PortForwarder");
        return true;
    }
    
    return false;
}

void PortForwarder::onNetworkInterfacesChanged()
{
    if (m_networkManager) {
        const QString status = m_networkManager->getInterfaceStatus();
        LOG_INFO(QString("Network interfaces changed: %1").arg(status), "PortForwarder");
    }
    
    // Consider restarting forwarding if we have active sessions
    // This is commented out to avoid disruption, but can be enabled if needed
    // restartAllForwarding();
}

void PortForwarder::onWireGuardStateChanged(bool active)
{
    const QString state = active ? "ACTIVE" : "INACTIVE";
    LOG_INFO(QString("WireGuard state changed to %1").arg(state), "PortForwarder");
    
    if (m_networkManager) {
        const QHostAddress wgAddress = m_networkManager->getWireGuardAddress();
        LOG_INFO(QString("WireGuard address: %1").arg(wgAddress.toString()), "PortForwarder");
    }
    
    // Optionally restart all forwarding when WireGuard state changes
    // This ensures we bind to the new WireGuard interface if it becomes available
    if (active && !m_sessions.isEmpty()) {
        LOG_INFO("WireGuard activated - restarting port forwarding to ensure proper binding", "PortForwarder");
        QTimer::singleShot(1000, this, &PortForwarder::restartAllForwarding); // Small delay to let interface stabilize
    }
}

void PortForwarder::restartAllForwarding()
{
    if (m_sessions.isEmpty()) return;
    
    LOG_INFO("Restarting all port forwarding sessions", "PortForwarder");
    
    // Save current camera configurations
    QList<CameraConfig> cameras;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        cameras.append(it.value()->camera);
    }
    
    // Stop all current forwarding
    stopAllForwarding();
    
    // Restart forwarding for each camera
    for (const CameraConfig& camera : cameras) {
        if (camera.isEnabled()) {
            QTimer::singleShot(500, this, [this, camera]() {
                startForwarding(camera);
            });
        }
    }
}
