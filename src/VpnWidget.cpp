#include "VpnWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
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
#include "AuthDialog.h"

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
    m_statusUpdateTimer->start();
    
    // Try to load saved config automatically
    QString savedConfig = getSavedWireGuardConfig();
    if (!savedConfig.isEmpty()) {
        QString configPath = getWireGuardConfigPath();
        QFileInfo fileInfo(configPath);
        m_loadedConfigPath = configPath;
        m_configPathLabel->setText(QString("Auto-loaded: %1").arg(fileInfo.fileName()));
        m_configPathLabel->setStyleSheet("font-style: normal; color: green;");
        m_currentConfigLabel->setText(tr("Configuration: %1 (Auto)").arg(fileInfo.fileName()));
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

void VpnWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    
    setupConfigGroup();
    setupConnectionGroup();
    setupStatusGroup();
    setupPingTestGroup();
    
    m_mainLayout->addStretch();
}

void VpnWidget::setupConfigGroup()
{
    m_configGroup = new QGroupBox(tr("VPN Configuration"));
    
    m_loadConfigButton = new QPushButton(tr("Fetch Config from Server"));
    m_configPathLabel = new QLabel(tr("No configuration loaded."));
    m_configPathLabel->setStyleSheet("font-style: italic; color: #888;");
    
    QHBoxLayout* layout = new QHBoxLayout(m_configGroup);
    layout->addWidget(m_loadConfigButton);
    layout->addWidget(m_configPathLabel, 1);
    
    m_mainLayout->addWidget(m_configGroup);
}

void VpnWidget::setupConnectionGroup()
{
    m_connectionGroup = new QGroupBox(tr("Connection Control"));
    
    m_connectButton = new QPushButton(tr("Connect"));
    m_connectButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    
    m_disconnectButton = new QPushButton(tr("Disconnect"));
    m_disconnectButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    
    m_connectionIconLabel = new QLabel();
    m_connectionIconLabel->setFixedSize(16, 16);
    
    m_connectionStatusLabel = new QLabel(tr("Disconnected"));
    m_connectionStatusLabel->setStyleSheet("font-weight: bold;");
    
    m_connectionProgress = new QProgressBar();
    m_connectionProgress->setRange(0, 0); // Indeterminate
    m_connectionProgress->setVisible(false);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_connectionIconLabel);
    buttonLayout->addWidget(m_connectionStatusLabel);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_connectionGroup);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_connectionProgress);
    
    m_mainLayout->addWidget(m_connectionGroup);
}

void VpnWidget::setupStatusGroup()
{
    m_statusGroup = new QGroupBox(tr("Live Status"));
    
    m_currentConfigLabel = new QLabel(tr("Configuration: Not loaded"));
    m_uptimeLabel = new QLabel(tr("Uptime: --"));
    m_transferLabel = new QLabel(tr("Data Transfer: RX: -- / TX: --"));
    
    QVBoxLayout* layout = new QVBoxLayout(m_statusGroup);
    layout->addWidget(m_currentConfigLabel);
    layout->addWidget(m_uptimeLabel);
    layout->addWidget(m_transferLabel);
    
    m_mainLayout->addWidget(m_statusGroup);
}

void VpnWidget::setupPingTestGroup()
{
    m_pingTestGroup = new QGroupBox(tr("Connectivity Test"));
    
    m_pingTestButton = new QPushButton(tr("Run Ping Test (10.0.0.1)"));
    m_pingStatusLabel = new QLabel(tr("Status: Ready"));
    m_pingOutputText = new QTextEdit();
    m_pingOutputText->setReadOnly(true);
    m_pingOutputText->setFont(QFont("Consolas", 9));
    m_pingOutputText->setPlaceholderText("Ping results will appear here...");
    m_pingOutputText->setFixedHeight(100);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_pingTestButton);
    buttonLayout->addStretch();
    
    QVBoxLayout* layout = new QVBoxLayout(m_pingTestGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_pingStatusLabel);
    layout->addWidget(m_pingOutputText);
    
    m_mainLayout->addWidget(m_pingTestGroup);
}

void VpnWidget::connectSignals()
{
    // User actions
    connect(m_loadConfigButton, &QPushButton::clicked, this, &VpnWidget::onLoadConfigClicked);
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

void VpnWidget::onLoadConfigClicked()
{
    // Check if we have a valid auth token
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Authentication Required"), 
                           tr("Please authenticate first to fetch the WireGuard configuration."));
        return;
    }
    
    fetchWireGuardConfig();
}

void VpnWidget::onConnectClicked()
{
    if (m_loadedConfigPath.isEmpty()) {
        QMessageBox::warning(this, tr("No Configuration"), tr("Please load a WireGuard configuration file first."));
        return;
    }
    
    m_connectionProgress->setVisible(true);
    // Pass the full path to use custom config files
    if (!m_wireGuardManager->connectTunnel(m_loadedConfigPath)) {
        QMessageBox::critical(this, tr("Connection Error"), tr("Failed to connect using the configuration: %1").arg(m_loadedConfigPath));
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
        m_uptimeLabel->setText("Uptime: --");
        m_transferLabel->setText("Data Transfer: RX: -- / TX: --");
    }
}

void VpnWidget::updateConnectionStatus()
{
    if (m_wireGuardManager->getConnectionStatus() == WireGuardManager::Connected && m_connectionStartTime.isValid()) {
        qint64 seconds = m_connectionStartTime.secsTo(QDateTime::currentDateTime());
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        
        m_uptimeLabel->setText(QString("Uptime: %1:%2:%3")
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
        QMessageBox::information(this, "Not Connected", "Please connect to the VPN before running a ping test.");
        return;
    }
    
    if (m_pingProcess->state() != QProcess::NotRunning) {
        QMessageBox::warning(this, "Ping In Progress", "A ping test is already running.");
        return;
    }
    
    m_pingOutputText->clear();
    m_pingStatusLabel->setText("Status: Pinging...");
    m_pingTestButton->setEnabled(false);
    
    m_pingProcess->start("ping", {"-n", "4", "10.0.0.1"});
}

void VpnWidget::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString output = QString::fromLocal8Bit(m_pingProcess->readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(m_pingProcess->readAllStandardError());
    
    m_pingOutputText->setPlainText(output.trimmed() + "\n" + errorOutput.trimmed());
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_pingStatusLabel->setText("Status: ✓ Success");
        m_pingStatusLabel->setStyleSheet("color: green;");
    } else {
        m_pingStatusLabel->setText("Status: ✗ Failed");
        m_pingStatusLabel->setStyleSheet("color: red;");
    }
    
    m_pingTestButton->setEnabled(true);
}

void VpnWidget::onPingError(QProcess::ProcessError error)
{
    m_pingStatusLabel->setText("Status: ✗ Error");
    m_pingStatusLabel->setStyleSheet("color: red;");
    m_pingOutputText->setPlainText(QString("Ping process error: %1").arg(m_pingProcess->errorString()));
    m_pingTestButton->setEnabled(true);
}

void VpnWidget::updateUI()
{
    WireGuardManager::ConnectionStatus status = m_wireGuardManager->getConnectionStatus();
    bool isConnected = (status == WireGuardManager::Connected);
    bool isConnecting = (status == WireGuardManager::Connecting);
    bool isDisconnecting = (status == WireGuardManager::Disconnecting);
    bool hasConfig = !m_loadedConfigPath.isEmpty();

    m_connectButton->setEnabled(!isConnected && !isConnecting && !isDisconnecting && hasConfig);
    m_disconnectButton->setEnabled(isConnected || isConnecting || isDisconnecting);
    m_loadConfigButton->setEnabled(!isConnected && !isConnecting && !isDisconnecting);
    
    m_connectionProgress->setVisible(isConnecting || isDisconnecting);
    
    m_connectionStatusLabel->setText(getStatusText(status));
    m_connectionIconLabel->setPixmap(getStatusIcon(status));
    
    m_pingTestButton->setEnabled(isConnected);
}

QString VpnWidget::getStatusText(WireGuardManager::ConnectionStatus status)
{
    switch (status) {
        case WireGuardManager::Disconnected: return "Disconnected";
        case WireGuardManager::Connecting:   return "Connecting...";
        case WireGuardManager::Connected:    return "Connected";
        case WireGuardManager::Disconnecting:return "Disconnecting...";
        case WireGuardManager::Error:        return "Error";
        default:                             return "Unknown";
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
                           tr("No valid authentication token found."));
        return;
    }
    
    m_configPathLabel->setText("Fetching configuration from server...");
    m_configPathLabel->setStyleSheet("font-style: italic; color: blue;");
    m_loadConfigButton->setEnabled(false);
    
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
    
    m_loadConfigButton->setEnabled(true);
    
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
            m_configPathLabel->setText(QString("Server config: %1").arg(fileInfo.fileName()));
            m_configPathLabel->setStyleSheet("font-style: normal; color: green;");
            m_currentConfigLabel->setText(tr("Configuration: %1 (Server)").arg(fileInfo.fileName()));
            
            emit logMessage(QString("Successfully fetched and saved WireGuard config: %1").arg(configPath));
            updateUI();
        } else {
            m_configPathLabel->setText("Error: No config content in server response");
            m_configPathLabel->setStyleSheet("font-style: italic; color: red;");
            QMessageBox::warning(this, tr("Config Error"), tr("Server response did not contain config content."));
        }
    } else if (statusCode == 401) {
        m_configPathLabel->setText("Authentication failed");
        m_configPathLabel->setStyleSheet("font-style: italic; color: red;");
        QMessageBox::warning(this, tr("Authentication Failed"), 
                           tr("Authentication token is invalid or expired. Please login again."));
        AuthDialog::clearCurrentAuthToken();
    } else {
        m_configPathLabel->setText(QString("Server error (%1)").arg(statusCode));
        m_configPathLabel->setStyleSheet("font-style: italic; color: red;");
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
    
    m_loadConfigButton->setEnabled(true);
    m_configPathLabel->setText("Network error occurred");
    m_configPathLabel->setStyleSheet("font-style: italic; color: red;");
    
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
        file.close();
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