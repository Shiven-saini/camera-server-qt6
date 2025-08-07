#ifndef VPNWIDGET_H
#define VPNWIDGET_H

#include <QWidget>
#include <QDateTime>
#include "WireGuardManager.h" // Full include for WireGuardManager::ConnectionStatus enum

// Forward-declare Qt classes to reduce header dependencies and improve compile times
QT_BEGIN_NAMESPACE
class QGroupBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTextEdit;
class QProcess;
class QTimer;
class QVBoxLayout;
QT_END_NAMESPACE

class VpnWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VpnWidget(QWidget *parent = nullptr);
    ~VpnWidget();

signals:
    void statusChanged(const QString& status);
    void logMessage(const QString& message);

private slots:
    // User-initiated actions
    void onLoadConfigClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onPingTestClicked();

    // Slots for QProcess (Ping)
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPingError(QProcess::ProcessError error);
    
    // Slots for WireGuardManager signals
    void onConnectionStatusChanged(WireGuardManager::ConnectionStatus status);
    void onTransferStatsUpdated(uint64_t rxBytes, uint64_t txBytes);
    void onWireGuardError(const QString& error);
    void onWireGuardLogMessage(const QString& message);

    // Internal timer slot
    void updateConnectionStatus();

private:
    // UI Setup
    void setupUI();
    void setupConfigGroup();
    void setupConnectionGroup();
    void setupStatusGroup();
    void setupPingTestGroup();
    void connectSignals();

    // UI State Management
    void updateUI();
    QString getStatusText(WireGuardManager::ConnectionStatus status);
    QPixmap getStatusIcon(WireGuardManager::ConnectionStatus status);
    
    // Core components
    WireGuardManager* m_wireGuardManager;
    QTimer* m_statusUpdateTimer;
    QProcess* m_pingProcess;

    // UI Components (pointers managed by Qt's parent-child system)
    QVBoxLayout* m_mainLayout;
    
    QGroupBox* m_configGroup;
    QPushButton* m_loadConfigButton;
    QLabel* m_configPathLabel;
    
    QGroupBox* m_connectionGroup;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_connectionStatusLabel;
    QLabel* m_connectionIconLabel;
    QProgressBar* m_connectionProgress;
    
    QGroupBox* m_statusGroup;
    QLabel* m_currentConfigLabel;
    QLabel* m_uptimeLabel;
    QLabel* m_transferLabel;
    
    QGroupBox* m_pingTestGroup;
    QPushButton* m_pingTestButton;
    QLabel* m_pingStatusLabel;
    QTextEdit* m_pingOutputText;
    
    // State tracking
    QString m_loadedConfigPath;
    QDateTime m_connectionStartTime;
};

#endif // VPNWIDGET_H
