#ifndef PORTFORWARDER_H
#define PORTFORWARDER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QHash>
#include "CameraConfig.h"

class PortForwarder : public QObject
{
    Q_OBJECT

public:
    explicit PortForwarder(QObject *parent = nullptr);
    ~PortForwarder();
    
    bool startForwarding(const CameraConfig& camera);
    void stopForwarding(const QString& cameraId);
    void stopAllForwarding();
    
    bool isForwarding(const QString& cameraId) const;
    QStringList getActiveForwards() const;

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
    void handleReconnectTimer();

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
    
    QHash<QString, ForwardingSession*> m_sessions;
    QHash<QTcpSocket*, QString> m_socketToCameraMap;
};

#endif // PORTFORWARDER_H
