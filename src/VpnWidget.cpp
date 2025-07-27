#include "VpnWidget.h"
#include "WireGuardConfigDialog.h"
#include <QMessageBox>
#include <QDateTime>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QStyle>

VpnWidget::VpnWidget(QWidget *parent)
    : QWidget(parent)
    , m_wireGuardManager(new WireGuardManager(this))
    , m_statusUpdateTimer(new QTimer(this))
    , m_lastStatus(WireGuardManager::Disconnected)
    , m_isUpdatingConfigs(false)
{
    setupUI();
    connectSignals();
    
    // Setup status update timer
    m_statusUpdateTimer->setInterval(1000); // Update every second
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &VpnWidget::updateConnectionStatus);
    m_statusUpdateTimer->start();
    
    // Initial UI update
    refreshConfigsList();
    updateUI();
}

VpnWidget::~VpnWidget()
{
}

void VpnWidget::onConnectClicked()
{
    QString selectedConfig = m_configCombo->currentText();
    if (selectedConfig.isEmpty()) {
        QMessageBox::warning(this, "No Configuration", 
            "Please select a WireGuard configuration to connect.");
        return;
    }
    
    m_connectButton->setEnabled(false);
    m_connectionProgress->setVisible(true);
    
    // Use QTimer to defer the connection to avoid blocking the UI thread
    QTimer::singleShot(10, [this, selectedConfig]() {
        bool success = m_wireGuardManager->connectTunnel(selectedConfig);
        if (success) {
            m_connectionStartTime = QDateTime::currentDateTime();
            emit logMessage(QString("VPN: Connecting to WireGuard tunnel: %1").arg(selectedConfig));
        } else {
            // Re-enable button on failure
            m_connectButton->setEnabled(true);
            m_connectionProgress->setVisible(false);
            emit logMessage(QString("VPN Error: Failed to connect to WireGuard tunnel: %1").arg(selectedConfig));
        }
    });
}

void VpnWidget::onDisconnectClicked()
{
    m_disconnectButton->setEnabled(false);
    m_connectionProgress->setVisible(true);
    
    QString currentConfig = m_wireGuardManager->getCurrentConfigName();
    
    // Use QTimer to defer the disconnection to avoid blocking the UI thread
    QTimer::singleShot(10, [this, currentConfig]() {
        bool success = m_wireGuardManager->disconnectTunnel();
        if (success) {
            emit logMessage(QString("VPN: Disconnecting from WireGuard tunnel: %1").arg(currentConfig));
        } else {
            // Re-enable button on failure
            m_disconnectButton->setEnabled(true);
            m_connectionProgress->setVisible(false);
            emit logMessage(QString("VPN Error: Failed to disconnect from WireGuard tunnel: %1").arg(currentConfig));
        }
    });
}

void VpnWidget::onCreateConfigClicked()
{
    WireGuardConfigDialog dialog(m_wireGuardManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        WireGuardConfig config = dialog.getConfiguration();
        if (m_wireGuardManager->saveConfig(config)) {
            refreshConfigsList();
            
            // Select the newly created config
            int index = m_configCombo->findText(config.interfaceConfig.name);
            if (index >= 0) {
                m_configCombo->setCurrentIndex(index);
            }
            
            emit logMessage(QString("Created new VPN configuration: %1").arg(config.interfaceConfig.name));
        }
    }
}

void VpnWidget::onEditConfigClicked()
{
    QString selectedConfig = m_configCombo->currentText();
    if (selectedConfig.isEmpty()) {
        QMessageBox::warning(this, "No Configuration", 
            "Please select a configuration to edit.");
        return;
    }
    
    WireGuardConfig config = m_wireGuardManager->loadConfig(selectedConfig);
    if (config.interfaceConfig.name.isEmpty()) {
        QMessageBox::warning(this, "Load Error", 
            "Failed to load the selected configuration.");
        return;
    }
    
    WireGuardConfigDialog dialog(m_wireGuardManager, config, this);
    if (dialog.exec() == QDialog::Accepted) {
        WireGuardConfig updatedConfig = dialog.getConfiguration();
        if (m_wireGuardManager->saveConfig(updatedConfig)) {
            refreshConfigsList();
            emit logMessage(QString("Updated VPN configuration: %1").arg(updatedConfig.interfaceConfig.name));
        }
    }
}

void VpnWidget::onDeleteConfigClicked()
{
    QString selectedConfig = m_configCombo->currentText();
    if (selectedConfig.isEmpty()) {
        QMessageBox::warning(this, "No Configuration", 
            "Please select a configuration to delete.");
        return;
    }
    
    // Check if the configuration is currently connected
    if (m_wireGuardManager->getCurrentConfigName() == selectedConfig && 
        m_wireGuardManager->getConnectionStatus() == WireGuardManager::Connected) {
        QMessageBox::warning(this, "Configuration In Use", 
            "Cannot delete a configuration that is currently connected. Please disconnect first.");
        return;
    }
    
    int ret = QMessageBox::question(this, "Delete Configuration",
        QString("Are you sure you want to delete the configuration '%1'?").arg(selectedConfig),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        if (m_wireGuardManager->deleteConfig(selectedConfig)) {
            refreshConfigsList();
            emit logMessage(QString("Deleted VPN configuration: %1").arg(selectedConfig));
        }
    }
}

void VpnWidget::onRefreshConfigsClicked()
{
    refreshConfigsList();
    emit logMessage("Refreshed VPN configurations list");
}

void VpnWidget::onConfigSelectionChanged()
{
    if (!m_isUpdatingConfigs) {
        updateUI();
    }
}

void VpnWidget::onConnectionStatusChanged(WireGuardManager::ConnectionStatus status)
{
    m_lastStatus = status;
    updateUI();
    
    QString statusText = getStatusText(status);
    emit statusChanged(statusText);
    
    // Hide progress bar when connection state is stable
    if (status == WireGuardManager::Connected || 
        status == WireGuardManager::Disconnected || 
        status == WireGuardManager::Error) {
        m_connectionProgress->setVisible(false);
    }
}

void VpnWidget::onTransferStatsUpdated(uint64_t rxBytes, uint64_t txBytes)
{
    updateTransferStats(rxBytes, txBytes);
}

void VpnWidget::onConfigurationChanged()
{
    refreshConfigsList();
}

void VpnWidget::onWireGuardError(const QString& error)
{
    QMessageBox::critical(this, "WireGuard Error", error);
    emit logMessage(QString("VPN Error: %1").arg(error));
}

void VpnWidget::onWireGuardLogMessage(const QString& message)
{
    emit logMessage(QString("VPN: %1").arg(message));
}

void VpnWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    
    setupConnectionGroup();
    setupConfigGroup();
    setupStatusGroup();
    
    // Add stretch to push everything to the top
    m_mainLayout->addStretch();
}

void VpnWidget::setupConnectionGroup()
{
    m_connectionGroup = new QGroupBox("VPN Connection");
    m_mainLayout->addWidget(m_connectionGroup);
    
    QVBoxLayout* groupLayout = new QVBoxLayout(m_connectionGroup);
    
    // Configuration selection
    QHBoxLayout* configLayout = new QHBoxLayout;
    configLayout->addWidget(new QLabel("Configuration:"));
    
    m_configCombo = new QComboBox;
    m_configCombo->setMinimumWidth(150);
    configLayout->addWidget(m_configCombo, 1);
    
    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setMaximumWidth(80);
    configLayout->addWidget(m_refreshButton);
    
    groupLayout->addLayout(configLayout);
    
    // Connection controls
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_connectButton = new QPushButton("Connect");
    m_connectButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    buttonLayout->addWidget(m_connectButton);
    
    m_disconnectButton = new QPushButton("Disconnect");
    m_disconnectButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    m_disconnectButton->setEnabled(false);
    buttonLayout->addWidget(m_disconnectButton);
    
    groupLayout->addLayout(buttonLayout);
    
    // Connection status
    QHBoxLayout* statusLayout = new QHBoxLayout;
    
    m_connectionIconLabel = new QLabel;
    m_connectionIconLabel->setFixedSize(16, 16);
    statusLayout->addWidget(m_connectionIconLabel);
    
    m_connectionStatusLabel = new QLabel("Disconnected");
    m_connectionStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(m_connectionStatusLabel);
    
    statusLayout->addStretch();
    groupLayout->addLayout(statusLayout);
    
    // Progress bar (initially hidden)
    m_connectionProgress = new QProgressBar;
    m_connectionProgress->setRange(0, 0); // Indeterminate progress
    m_connectionProgress->setVisible(false);
    groupLayout->addWidget(m_connectionProgress);
}

void VpnWidget::setupConfigGroup()
{
    m_configGroup = new QGroupBox("Configuration Management");
    m_mainLayout->addWidget(m_configGroup);
    
    QVBoxLayout* groupLayout = new QVBoxLayout(m_configGroup);
    
    // Configuration buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_createConfigButton = new QPushButton("Create New");
    buttonLayout->addWidget(m_createConfigButton);
    
    m_editConfigButton = new QPushButton("Edit");
    buttonLayout->addWidget(m_editConfigButton);
    
    m_deleteConfigButton = new QPushButton("Delete");
    buttonLayout->addWidget(m_deleteConfigButton);
    
    buttonLayout->addStretch();
    groupLayout->addLayout(buttonLayout);
    
    // Configuration info
    m_configCountLabel = new QLabel("0 configurations available");
    m_configCountLabel->setStyleSheet("color: #666; font-size: 11px;");
    groupLayout->addWidget(m_configCountLabel);
}

void VpnWidget::setupStatusGroup()
{
    m_statusGroup = new QGroupBox("Connection Status");
    m_mainLayout->addWidget(m_statusGroup);
    
    QVBoxLayout* groupLayout = new QVBoxLayout(m_statusGroup);
    
    // Current configuration
    m_currentConfigLabel = new QLabel("Not connected");
    m_currentConfigLabel->setStyleSheet("font-weight: bold;");
    groupLayout->addWidget(m_currentConfigLabel);
    
    // Uptime
    m_uptimeLabel = new QLabel("Uptime: --");
    groupLayout->addWidget(m_uptimeLabel);
    
    // Transfer statistics
    m_transferLabel = new QLabel("Data: RX: -- / TX: --");
    groupLayout->addWidget(m_transferLabel);
}

void VpnWidget::connectSignals()
{
    // Connection controls
    connect(m_connectButton, &QPushButton::clicked, this, &VpnWidget::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &VpnWidget::onDisconnectClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &VpnWidget::onRefreshConfigsClicked);
    connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &VpnWidget::onConfigSelectionChanged);
    
    // Configuration management
    connect(m_createConfigButton, &QPushButton::clicked, this, &VpnWidget::onCreateConfigClicked);
    connect(m_editConfigButton, &QPushButton::clicked, this, &VpnWidget::onEditConfigClicked);
    connect(m_deleteConfigButton, &QPushButton::clicked, this, &VpnWidget::onDeleteConfigClicked);
    
    // WireGuard Manager signals
    connect(m_wireGuardManager, &WireGuardManager::connectionStatusChanged,
            this, &VpnWidget::onConnectionStatusChanged);
    connect(m_wireGuardManager, &WireGuardManager::transferStatsUpdated,
            this, &VpnWidget::onTransferStatsUpdated);
    connect(m_wireGuardManager, &WireGuardManager::configurationChanged,
            this, &VpnWidget::onConfigurationChanged);
    connect(m_wireGuardManager, &WireGuardManager::errorOccurred,
            this, &VpnWidget::onWireGuardError);
    connect(m_wireGuardManager, &WireGuardManager::logMessage,
            this, &VpnWidget::onWireGuardLogMessage);
}

void VpnWidget::updateUI()
{
    WireGuardManager::ConnectionStatus status = m_wireGuardManager->getConnectionStatus();
    QString currentConfig = m_wireGuardManager->getCurrentConfigName();
    bool hasConfigs = m_configCombo->count() > 0;
    bool hasSelection = !m_configCombo->currentText().isEmpty();
    
    // Update connection controls
    m_connectButton->setEnabled(hasSelection && 
                               (status == WireGuardManager::Disconnected || 
                                status == WireGuardManager::Error));
    
    m_disconnectButton->setEnabled(status == WireGuardManager::Connected || 
                                  status == WireGuardManager::Connecting);
    
    // Update configuration management buttons
    m_editConfigButton->setEnabled(hasSelection);
    m_deleteConfigButton->setEnabled(hasSelection && 
                                    (currentConfig != m_configCombo->currentText() || 
                                     status == WireGuardManager::Disconnected));
    
    // Update status display
    QString statusText = getStatusText(status);
    m_connectionStatusLabel->setText(statusText);
    
    QPixmap statusIcon = getStatusIcon(status);
    m_connectionIconLabel->setPixmap(statusIcon);
    
    // Update current configuration display
    if (status == WireGuardManager::Connected && !currentConfig.isEmpty()) {
        m_currentConfigLabel->setText(QString("Connected to: %1").arg(currentConfig));
    } else {
        m_currentConfigLabel->setText("Not connected");
    }
}

void VpnWidget::updateConnectionStatus()
{
    // Update uptime if connected
    if (m_wireGuardManager->getConnectionStatus() == WireGuardManager::Connected && 
        m_connectionStartTime.isValid()) {
        
        qint64 seconds = m_connectionStartTime.secsTo(QDateTime::currentDateTime());
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        
        QString uptimeText = QString("Uptime: %1:%2:%3")
                           .arg(hours, 2, 10, QChar('0'))
                           .arg(minutes, 2, 10, QChar('0'))
                           .arg(secs, 2, 10, QChar('0'));
        
        m_uptimeLabel->setText(uptimeText);
    } else {
        m_uptimeLabel->setText("Uptime: --");
    }
}

void VpnWidget::updateTransferStats(uint64_t rxBytes, uint64_t txBytes)
{
    QString rxText = m_wireGuardManager->formatBytes(rxBytes);
    QString txText = m_wireGuardManager->formatBytes(txBytes);
    
    m_transferLabel->setText(QString("Data: RX: %1 / TX: %2").arg(rxText, txText));
}

void VpnWidget::refreshConfigsList()
{
    m_isUpdatingConfigs = true;
    
    QString currentSelection = m_configCombo->currentText();
    m_configCombo->clear();
    
    QStringList configs = m_wireGuardManager->getAvailableConfigs();
    m_configCombo->addItems(configs);
    
    // Restore selection if possible
    int index = m_configCombo->findText(currentSelection);
    if (index >= 0) {
        m_configCombo->setCurrentIndex(index);
    }
    
    // Update config count label
    m_configCountLabel->setText(QString("%1 configuration(s) available").arg(configs.size()));
    
    m_isUpdatingConfigs = false;
    updateUI();
}

QString VpnWidget::getStatusText(WireGuardManager::ConnectionStatus status)
{
    switch (status) {
        case WireGuardManager::Disconnected:
            return "Disconnected";
        case WireGuardManager::Connecting:
            return "Connecting...";
        case WireGuardManager::Connected:
            return "Connected";
        case WireGuardManager::Disconnecting:
            return "Disconnecting...";
        case WireGuardManager::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

QPixmap VpnWidget::getStatusIcon(WireGuardManager::ConnectionStatus status)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor color;
    switch (status) {
        case WireGuardManager::Connected:
            color = QColor("#4CAF50"); // Green
            break;
        case WireGuardManager::Connecting:
        case WireGuardManager::Disconnecting:
            color = QColor("#FF9800"); // Orange
            break;
        case WireGuardManager::Error:
            color = QColor("#f44336"); // Red
            break;
        case WireGuardManager::Disconnected:
        default:
            color = QColor("#9E9E9E"); // Gray
            break;
    }
    
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, 12, 12);
    
    return pixmap;
}
