#ifndef PORTFORWARDER_H
#define PORTFORWARDER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QHash>
#include <QHostAddress>
#include "CameraConfig.h"

class NetworkInterfaceManager;

class PortForwarder : public QObject
{
    Q_OBJECT

public:
    explicit PortForwarder(QObject *parent = nullptr);    ~PortForwarder();
    
    bool startForwarding(const CameraConfig& camera);
    void stopForwarding(const QString& cameraId);
    void stopAllForwarding();
    
    bool isForwarding(const QString& cameraId) const;
    QStringList getActiveForwards() const;

    // Network interface management
    void setNetworkInterfaceManager(NetworkInterfaceManager* manager);
    NetworkInterfaceManager* networkInterfaceManager() const;

signals:
    void forwardingStarted(const QString& cameraId, int externalPort);
    void forwardingStopped(const QString& cameraId);
    void forwardingError(const QString& cameraId, const QString& error);
    void connectionEstablished(const QString& cameraId, const QString& clientAddress);
    void connectionClosed(const QString& cameraId, const QString& clientAddress);

private slots:
    void handleNewConnection();
    void handleClientDisconnected();
    void handleClientDataReady();
    void handleTargetConnected();
    void handleTargetDisconnected();
    void handleTargetDataReady();
    void handleConnectionError(QAbstractSocket::SocketError error);
    void handleReconnectTimer();    void onNetworkInterfacesChanged();
    void onWireGuardStateChanged(bool active);

private:
    struct ForwardingSession {
        QTcpServer* server;
        CameraConfig camera;
        QHash<QTcpSocket*, QTcpSocket*> connections; // client -> target mapping
        QTimer* reconnectTimer;
        bool isReconnecting;
    };
    
    void setupReconnectTimer(const QString& cameraId);
    void cleanupSession(const QString& cameraId);
    void forwardData(QTcpSocket* from, QTcpSocket* to);
    bool bindToAllInterfaces(QTcpServer* server, quint16 port);
    void restartAllForwarding();
    
    QHash<QString, ForwardingSession*> m_sessions;
    QHash<QTcpSocket*, QString> m_socketToCameraMap;
    NetworkInterfaceManager* m_networkManager;
};

#endif // PORTFORWARDER_H
