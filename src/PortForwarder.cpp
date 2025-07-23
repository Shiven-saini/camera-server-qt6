#include "PortForwarder.h"
#include "Logger.h"
#include <QNetworkProxy>
#include <QTimer>

PortForwarder::PortForwarder(QObject *parent)
    : QObject(parent)
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
    
    // Start listening on the external port
    if (!session->server->listen(QHostAddress::Any, camera.externalPort())) {
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
