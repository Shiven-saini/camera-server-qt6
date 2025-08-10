#include "VpnWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include "AuthDialog.h"
#include "MainWindow.h"
#include "Logger.h"

VpnWidget::VpnWidget(QWidget *parent)
    : QWidget(parent)
    , m_wireGuardManager(new WireGuardManager(this))
    , m_statusUpdateTimer(new QTimer(this))
    , m_pingProcess(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_configReply(nullptr)
{
    setupUI();
    connectSignals();
    
    m_statusUpdateTimer->setInterval(1000); // 1-second updates
    m_statusUpdateTimer->start();    // Try to load saved config automatically
    QString savedConfig = getSavedWireGuardConfig();
    if (!savedConfig.isEmpty()) {
        QString configPath = getWireGuardConfigPath();
        QFileInfo fileInfo(configPath);
        m_loadedConfigPath = configPath;
        m_currentConfigLabel->setText(tr("Configuration: Ready for secure connection"));
        emit logMessage(QString("Auto-loaded WireGuard config: %1").arg(configPath));
    }
    
    updateUI();
}

VpnWidget::~VpnWidget()
{
    if (m_configReply) {
        m_configReply->abort();
        m_configReply->deleteLater();
    }
    
    if (m_pingProcess && m_pingProcess->state() != QProcess::NotRunning) {
        m_pingProcess->kill();
        m_pingProcess->waitForFinished(2000);
    }
}

// Public methods for external access
WireGuardManager::ConnectionStatus VpnWidget::getConnectionStatus() const
{
    return m_wireGuardManager->getConnectionStatus();
}

QString VpnWidget::getCurrentConfigName() const
{
    return m_wireGuardManager->getCurrentConfigName();
}

bool VpnWidget::isConnected() const
{
    return m_wireGuardManager->getConnectionStatus() == WireGuardManager::Connected;
}

void VpnWidget::connectToNetwork()
{
    if (getConnectionStatus() == WireGuardManager::Disconnected) {
        onConnectClicked();
    }
}

void VpnWidget::disconnectFromNetwork()
{
    if (getConnectionStatus() == WireGuardManager::Connected || 
        getConnectionStatus() == WireGuardManager::Connecting) {
        onDisconnectClicked();
    }
}

void VpnWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    
    setupConnectionGroup();
    setupStatusGroup();
    setupPingTestGroup();
    
    m_mainLayout->addStretch();
}

void VpnWidget::setupConnectionGroup()
{
    m_connectionGroup = new QGroupBox(tr("Remote Secure Access"));
    
    m_connectButton = new QPushButton(tr("Join Network"));
    m_connectButton->setMinimumHeight(32);
    m_connectButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"
    );
      m_disconnectButton = new QPushButton(tr("Leave Network"));
    m_disconnectButton->setMinimumHeight(32);
    m_disconnectButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #f44336;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #da190b;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"    );
    
    // Status indicator with icon and text
    QWidget* statusWidget = new QWidget();
    QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    
    m_connectionIconLabel = new QLabel();
    m_connectionIconLabel->setFixedSize(16, 16);
    
    m_connectionStatusLabel = new QLabel(tr("Disconnected"));
    m_connectionStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    
    statusLayout->addWidget(m_connectionIconLabel);
    statusLayout->addWidget(m_connectionStatusLabel);
    statusLayout->addStretch();
    
    m_connectionProgress = new QProgressBar();
    m_connectionProgress->setRange(0, 0); // Indeterminate
    m_connectionProgress->setVisible(false);
    m_connectionProgress->setMaximumHeight(20);
    m_connectionProgress->setStyleSheet(
        "QProgressBar {"
        "    border: 2px solid #cccccc;"
        "    border-radius: 5px;"
        "    text-align: center;"
        "    font-size: 10px;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #4CAF50;"
        "    border-radius: 3px;"
        "}"
    );      QVBoxLayout* mainLayout = new QVBoxLayout(m_connectionGroup);
    mainLayout->setSpacing(10);
      // Main button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(statusWidget);
    mainLayout->addWidget(m_connectionProgress);
    
    m_mainLayout->addWidget(m_connectionGroup);
}

void VpnWidget::setupStatusGroup()
{
    m_statusGroup = new QGroupBox(tr("Connection Details"));
    
    // Create a modern card-like layout
    QWidget* statusCard = new QWidget();
    statusCard->setStyleSheet(
        "QWidget {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #e9ecef;"
        "    border-radius: 6px;"
        "    padding: 8px;"
        "}"
    );
    
    m_currentConfigLabel = new QLabel(tr("Configuration: Not loaded"));
    m_currentConfigLabel->setStyleSheet("color: #495057; font-size: 11px;");
    
    m_uptimeLabel = new QLabel(tr("Session Duration: --"));
    m_uptimeLabel->setStyleSheet("color: #495057; font-size: 11px;");
    
    m_transferLabel = new QLabel(tr("Data Transfer: RX: -- / TX: --"));
    m_transferLabel->setStyleSheet("color: #495057; font-size: 11px;");
    
    QVBoxLayout* cardLayout = new QVBoxLayout(statusCard);
    cardLayout->setSpacing(6);
    cardLayout->addWidget(m_currentConfigLabel);
    cardLayout->addWidget(m_uptimeLabel);
    cardLayout->addWidget(m_transferLabel);
    
    QVBoxLayout* layout = new QVBoxLayout(m_statusGroup);
    layout->addWidget(statusCard);
    
    m_mainLayout->addWidget(m_statusGroup);
}

void VpnWidget::setupPingTestGroup()
{
    m_pingTestGroup = new QGroupBox(tr("Network Connectivity Test"));
    
    m_pingTestButton = new QPushButton(tr("Test Connection"));
    m_pingTestButton->setMinimumHeight(30);
    m_pingTestButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #007bff;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 5px;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056b3;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"
    );
    
    // Status indicator for ping test
    QWidget* pingStatusWidget = new QWidget();
    pingStatusWidget->setStyleSheet(
        "QWidget {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #e9ecef;"
        "    border-radius: 5px;"
        "    padding: 6px;"
        "}"
    );
    
    m_pingStatusLabel = new QLabel(tr("Status: Ready to test"));
    m_pingStatusLabel->setStyleSheet("color: #495057; font-weight: 500; font-size: 11px;");
    
    QHBoxLayout* pingStatusLayout = new QHBoxLayout(pingStatusWidget);
    pingStatusLayout->setContentsMargins(6, 3, 6, 3);
    pingStatusLayout->addWidget(m_pingStatusLabel);
    
    // Remove the detailed ping output text area - we'll only show status
    
    QVBoxLayout* layout = new QVBoxLayout(m_pingTestGroup);
    layout->setSpacing(8);
    layout->addWidget(m_pingTestButton);
    layout->addWidget(pingStatusWidget);
    
    m_mainLayout->addWidget(m_pingTestGroup);
}

void VpnWidget::connectSignals()
{    // User actions
    connect(m_connectButton, &QPushButton::clicked, this, &VpnWidget::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &VpnWidget::onDisconnectClicked);
    connect(m_pingTestButton, &QPushButton::clicked, this, &VpnWidget::onPingTestClicked);
    
    // WireGuard manager signals
    connect(m_wireGuardManager, &WireGuardManager::connectionStatusChanged, this, &VpnWidget::onConnectionStatusChanged);
    connect(m_wireGuardManager, &WireGuardManager::transferStatsUpdated, this, &VpnWidget::onTransferStatsUpdated);
    connect(m_wireGuardManager, &WireGuardManager::errorOccurred, this, &VpnWidget::onWireGuardError);
    connect(m_wireGuardManager, &WireGuardManager::logMessage, this, &VpnWidget::onWireGuardLogMessage);
    
    // Ping process signals
    if (!m_pingProcess) {
        m_pingProcess = new QProcess(this);
        connect(m_pingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &VpnWidget::onPingFinished);
        connect(m_pingProcess, &QProcess::errorOccurred, this, &VpnWidget::onPingError);
    }
    
    // Timer signal
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &VpnWidget::updateConnectionStatus);
}

void VpnWidget::onConnectClicked()
{    // Check if we already have a valid config
    if (m_loadedConfigPath.isEmpty() || !QFile::exists(m_loadedConfigPath)) {
        // No config available, need to fetch from server first
        QString token = AuthDialog::getCurrentAuthToken();
        if (token.isEmpty()) {
            QMessageBox::warning(this, tr("Authentication Required"), 
                               tr("You are not authenticated or your session has expired. Please restart the application to log in again."));
            return;
        }
        
        // Fetch config first, then connect (keep button text as "Join Network")
        m_connectButton->setEnabled(false);
        fetchWireGuardConfig();
        return;
    }
    
    // Config is available, proceed with connection
    m_connectionProgress->setVisible(true);
    m_connectButton->setText("Joining Network...");
    m_connectButton->setEnabled(false);
    
    // Pass the full path to use custom config files
    if (!m_wireGuardManager->connectTunnel(m_loadedConfigPath)) {
        QMessageBox::critical(this, tr("Connection Error"), tr("Failed to connect using the configuration: %1").arg(m_loadedConfigPath));
        m_connectButton->setText("Join Network");
        m_connectButton->setEnabled(true);
        m_connectionProgress->setVisible(false);
    }
    updateUI();
}

void VpnWidget::onDisconnectClicked()
{
    m_connectionProgress->setVisible(true);
    m_wireGuardManager->disconnectTunnel();
    updateUI();
}

void VpnWidget::onConnectionStatusChanged(WireGuardManager::ConnectionStatus status)
{
    updateUI();
    
    if (status == WireGuardManager::Connected) {
        m_connectionStartTime = QDateTime::currentDateTime();
    } else {
        m_connectionStartTime = QDateTime(); // Invalidate time
        m_uptimeLabel->setText("Session Duration: --");
        m_transferLabel->setText("Data Transfer: RX: -- / TX: --");
    }
    
    // Emit status change signal for external components (like SystemTrayManager)
    QString statusText;
    QString configName = getCurrentConfigName();
    
    switch (status) {
        case WireGuardManager::Connected:
            statusText = configName.isEmpty() ? "Connected" : QString("Connected (%1)").arg(configName);
            break;
        case WireGuardManager::Connecting:
            statusText = "Connecting...";
            break;
        case WireGuardManager::Disconnected:
            statusText = "Disconnected";
            break;
        case WireGuardManager::Disconnecting:
            statusText = "Disconnecting...";
            break;
        default:
            statusText = "Unknown";
            break;
    }
    
    emit statusChanged(statusText);
}

void VpnWidget::updateConnectionStatus()
{
    if (m_wireGuardManager->getConnectionStatus() == WireGuardManager::Connected && m_connectionStartTime.isValid()) {
        qint64 seconds = m_connectionStartTime.secsTo(QDateTime::currentDateTime());
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        
        m_uptimeLabel->setText(QString("Session Duration: %1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0')));
    }
}

void VpnWidget::onTransferStatsUpdated(uint64_t rxBytes, uint64_t txBytes)
{
    QString rxStr = m_wireGuardManager->formatBytes(rxBytes);
    QString txStr = m_wireGuardManager->formatBytes(txBytes);
    m_transferLabel->setText(QString("Data Transfer: RX: %1 / TX: %2").arg(rxStr, txStr));
}

void VpnWidget::onWireGuardError(const QString& error)
{
    QMessageBox::critical(this, "WireGuard Error", error);
    emit logMessage(QString("WireGuard Error: %1").arg(error));
    updateUI();
}

void VpnWidget::onWireGuardLogMessage(const QString& message)
{
    emit logMessage(message);
}

void VpnWidget::onPingTestClicked()
{
    if (m_wireGuardManager->getConnectionStatus() != WireGuardManager::Connected) {
        QMessageBox::information(this, "Not Connected", "Please join the network before running a connectivity test.");
        return;
    }
    
    if (m_pingProcess->state() != QProcess::NotRunning) {
        QMessageBox::warning(this, "Test In Progress", "A connectivity test is already running.");
        return;
    }
    
    m_pingStatusLabel->setText("Status: Testing network connectivity...");
    m_pingStatusLabel->setStyleSheet("color: #007bff; font-weight: 500; font-size: 11px;");
    m_pingTestButton->setEnabled(false);
    
    m_pingProcess->start("ping", {"-n", "4", "10.0.0.1"});
}

void VpnWidget::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString output = QString::fromLocal8Bit(m_pingProcess->readAllStandardOutput());
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        // Extract response time from ping output for a more detailed status
        QRegularExpression timeRegex(R"(time[<=](\d+)ms)");
        QRegularExpressionMatch match = timeRegex.match(output);
        
        if (match.hasMatch()) {
            QString responseTime = match.captured(1);
            m_pingStatusLabel->setText(QString("Status: ✓ Connected (Response: %1ms)").arg(responseTime));
        } else {
            m_pingStatusLabel->setText("Status: ✓ Network connectivity confirmed");
        }
        m_pingStatusLabel->setStyleSheet("color: #28a745; font-weight: 500; font-size: 11px;");
    } else {
        m_pingStatusLabel->setText("Status: ✗ Network connectivity issues detected");
        m_pingStatusLabel->setStyleSheet("color: #dc3545; font-weight: 500; font-size: 11px;");
    }
    
    m_pingTestButton->setEnabled(true);
}

void VpnWidget::onPingError(QProcess::ProcessError error)
{
    m_pingStatusLabel->setText("Status: ✗ Network test failed");
    m_pingStatusLabel->setStyleSheet("color: #dc3545; font-weight: 500; font-size: 11px;");
    m_pingTestButton->setEnabled(true);
}

void VpnWidget::updateUI()
{
    WireGuardManager::ConnectionStatus status = m_wireGuardManager->getConnectionStatus();
    bool isConnected = (status == WireGuardManager::Connected);
    bool isConnecting = (status == WireGuardManager::Connecting);
    bool isDisconnecting = (status == WireGuardManager::Disconnecting);

    // Update connect button text based on status
    if (isConnected) {
        m_connectButton->setText("Connected to Network");
        m_connectButton->setEnabled(false);
    } else if (isConnecting) {
        m_connectButton->setText("Joining Network...");
        m_connectButton->setEnabled(false);
    } else {
        m_connectButton->setText("Join Network");
        m_connectButton->setEnabled(!isDisconnecting);
    }

    m_disconnectButton->setEnabled(isConnected || isConnecting || isDisconnecting);
    
    m_connectionProgress->setVisible(isConnecting || isDisconnecting);
    
    m_connectionStatusLabel->setText(getStatusText(status));
    m_connectionIconLabel->setPixmap(getStatusIcon(status));
    
    m_pingTestButton->setEnabled(isConnected);
}

QString VpnWidget::getStatusText(WireGuardManager::ConnectionStatus status)
{
    switch (status) {
        case WireGuardManager::Disconnected: return "Not Connected";
        case WireGuardManager::Connecting:   return "Joining Network...";
        case WireGuardManager::Connected:    return "Connected to Secure Network";
        case WireGuardManager::Disconnecting:return "Leaving Network...";
        case WireGuardManager::Error:        return "Connection Error";
        default:                             return "Unknown Status";
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
        case WireGuardManager::Connected:    color = QColor("#4CAF50"); break; // Green
        case WireGuardManager::Connecting:   
        case WireGuardManager::Disconnecting:color = QColor("#FFC107"); break; // Amber
        case WireGuardManager::Error:        color = QColor("#f44336"); break; // Red
        default:                             color = QColor("#9E9E9E"); break; // Grey
    }
    
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 16, 16);
    
    return pixmap;
}

void VpnWidget::fetchWireGuardConfig()
{
    if (m_configReply) {
        m_configReply->abort();
        m_configReply->deleteLater();
    }
      QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Authentication Required"), 
                           tr("You are not authenticated or your session has expired. Please restart the application to log in again."));
        m_connectButton->setEnabled(true);
        return;
    }
    
    QNetworkRequest request(QUrl("http://3.82.200.187:8086/wireguard/generate-config"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    m_configReply = m_networkManager->post(request, QByteArray());
    
    connect(m_configReply, &QNetworkReply::finished, this, &VpnWidget::onConfigFetchFinished);
    connect(m_configReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), 
            this, &VpnWidget::onConfigFetchError);
    
    emit logMessage("Fetching WireGuard configuration from server...");
}

void VpnWidget::onConfigFetchFinished()
{
    if (!m_configReply) return;
    
    int statusCode = m_configReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_configReply->readAll();
    m_configReply->deleteLater();
    m_configReply = nullptr;
    
    m_connectButton->setEnabled(true);
    
    if (statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString configContent = obj.value("config_content").toString();
          if (!configContent.isEmpty()) {
            // Replace \n with actual newlines for proper formatting
            configContent.replace("\\n", "\n");
            
            saveWireGuardConfig(configContent);
            
            QString configPath = getWireGuardConfigPath();
            QFileInfo fileInfo(configPath);
            m_loadedConfigPath = configPath;
            m_currentConfigLabel->setText(tr("Configuration: Ready for secure connection"));
            
            emit logMessage(QString("Successfully fetched and saved WireGuard config: %1").arg(configPath));
            
            // Validate config file is properly written and accessible before proceeding
            validateAndConnect();
            
        } else {
            QMessageBox::warning(this, tr("Config Error"), tr("Server response did not contain config content."));
        }
    } else if (statusCode == 401) {
        QMessageBox::warning(this, tr("Authentication Failed"), 
                           tr("Authentication token is invalid or expired. Please login again."));
        AuthDialog::clearCurrentAuthToken();
    } else {
        QMessageBox::warning(this, tr("Server Error"), 
                           tr("Failed to fetch configuration. Server returned status code: %1").arg(statusCode));
    }
}

void VpnWidget::onConfigFetchError(QNetworkReply::NetworkError error)
{
    if (!m_configReply) return;
    
    QString errorString = m_configReply->errorString();
    m_configReply->deleteLater();
    m_configReply = nullptr;
    
    m_connectButton->setEnabled(true);
    
    QMessageBox::critical(this, tr("Network Error"), 
                         tr("Failed to fetch configuration from server:\n%1").arg(errorString));
    
    emit logMessage(QString("Network error fetching config: %1").arg(errorString));
}

void VpnWidget::saveWireGuardConfig(const QString& configContent)
{
    // Save to QSettings for persistence
    QSettings settings("ViscoConnect", "WireGuard");
    settings.setValue("config_content", configContent);
    
    // Also save to a physical file for WireGuard to use
    QString configPath = getWireGuardConfigPath();
    QDir dir = QFileInfo(configPath).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
      QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << configContent;
        stream.flush(); // Ensure data is written to the stream buffer
        file.flush();   // Ensure data is written to the OS buffer  
        file.close();   // This also flushes and syncs to disk
        
        // Double-check that the file was written correctly
        if (file.exists() && file.size() > 0) {
            emit logMessage(QString("Config file saved successfully: %1 (%2 bytes)").arg(configPath).arg(file.size()));
        } else {
            emit logMessage(QString("WARNING: Config file may not have been saved properly: %1").arg(configPath));
        }
    } else {
        emit logMessage(QString("ERROR: Failed to open config file for writing: %1").arg(configPath));
    }
}

QString VpnWidget::getSavedWireGuardConfig()
{
    QSettings settings("ViscoConnect", "WireGuard");
    return settings.value("config_content").toString();
}

QString VpnWidget::getWireGuardConfigPath()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("wireguard_server.conf");
}

void VpnWidget::validateAndConnect()
{
    QString configPath = getWireGuardConfigPath();
    
    // Validate that the config file exists and is readable
    auto validateConfig = [this, configPath]() -> bool {
        QFile configFile(configPath);
        if (!configFile.exists()) {
            emit logMessage(QString("Config file does not exist: %1").arg(configPath));
            return false;
        }
        
        if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit logMessage(QString("Cannot read config file: %1").arg(configPath));
            return false;
        }
        
        QTextStream stream(&configFile);
        QString content = stream.readAll();
        configFile.close();
        
        if (content.isEmpty()) {
            emit logMessage(QString("Config file is empty: %1").arg(configPath));
            return false;
        }
        
        // Basic validation that this looks like a WireGuard config
        if (!content.contains("[Interface]") || !content.contains("PrivateKey")) {
            emit logMessage(QString("Config file does not appear to be a valid WireGuard configuration: %1").arg(configPath));
            return false;
        }
        
        emit logMessage(QString("Config file validation successful: %1 (%2 characters)").arg(configPath).arg(content.length()));
        return true;
    };
    
    // First validation attempt
    if (validateConfig()) {
        // Config is immediately available, proceed with connection
        emit logMessage("Config validated immediately, proceeding with connection");
        QMetaObject::invokeMethod(this, "onConnectClicked", Qt::QueuedConnection);
    } else {
        // Config not ready yet, wait a bit and retry
        emit logMessage("Config not immediately available, waiting 100ms before retry...");
        QTimer::singleShot(100, [this, validateConfig]() {
            if (validateConfig()) {
                emit logMessage("Config validated after retry, proceeding with connection");
                QMetaObject::invokeMethod(this, "onConnectClicked", Qt::QueuedConnection);
            } else {
                // Final attempt after a longer delay
                emit logMessage("Config still not available, waiting 500ms for final attempt...");
                QTimer::singleShot(500, [this, validateConfig]() {
                    if (validateConfig()) {
                        emit logMessage("Config validated after final retry, proceeding with connection");
                        QMetaObject::invokeMethod(this, "onConnectClicked", Qt::QueuedConnection);
                    } else {
                        emit logMessage("ERROR: Config file validation failed after all retries");
                        QMessageBox::critical(this, tr("Configuration Error"), 
                            tr("Failed to validate the configuration file. Please try again."));
                        m_connectButton->setEnabled(true);
                    }
                });
            }
        });
    }
}