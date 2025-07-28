#ifndef VPNWIDGET_H
#define VPNWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QFrame>
#include <QProcess>
#include <QTextEdit>
#include "WireGuardManager.h"

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
    void onConnectClicked();
    void onDisconnectClicked();
    void onCreateConfigClicked();
    void onEditConfigClicked();
    void onDeleteConfigClicked();    void onRefreshConfigsClicked();
    void onConfigSelectionChanged();
    void onPingTestClicked();
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPingError(QProcess::ProcessError error);
    
    // WireGuard Manager slots
    void onConnectionStatusChanged(WireGuardManager::ConnectionStatus status);
    void onTransferStatsUpdated(uint64_t rxBytes, uint64_t txBytes);
    void onConfigurationChanged();
    void onWireGuardError(const QString& error);
    void onWireGuardLogMessage(const QString& message);

private:
    void setupUI();    void setupConnectionGroup();
    void setupConfigGroup();
    void setupStatusGroup();
    void setupPingTestGroup();
    void connectSignals();
    void updateUI();
    void updateConnectionStatus();
    void updateTransferStats(uint64_t rxBytes, uint64_t txBytes);
    void refreshConfigsList();
    QString getStatusText(WireGuardManager::ConnectionStatus status);
    QPixmap getStatusIcon(WireGuardManager::ConnectionStatus status);
    
    // Core components
    WireGuardManager* m_wireGuardManager;
    QTimer* m_statusUpdateTimer;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    
    // Connection group
    QGroupBox* m_connectionGroup;
    QComboBox* m_configCombo;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_refreshButton;
    QLabel* m_connectionStatusLabel;
    QLabel* m_connectionIconLabel;
    
    // Configuration group
    QGroupBox* m_configGroup;
    QPushButton* m_createConfigButton;
    QPushButton* m_editConfigButton;
    QPushButton* m_deleteConfigButton;
    QLabel* m_configCountLabel;
      // Status group
    QGroupBox* m_statusGroup;
    QLabel* m_currentConfigLabel;
    QLabel* m_uptimeLabel;
    QLabel* m_transferLabel;
    QProgressBar* m_connectionProgress;
    
    // Ping test group
    QGroupBox* m_pingTestGroup;
    QPushButton* m_pingTestButton;
    QLabel* m_pingStatusLabel;
    QTextEdit* m_pingOutputText;
    QProcess* m_pingProcess;
    
    // Separator
    QFrame* m_separator;
    
    // State tracking
    WireGuardManager::ConnectionStatus m_lastStatus;
    QDateTime m_connectionStartTime;
    bool m_isUpdatingConfigs;
};

#endif // VPNWIDGET_H
