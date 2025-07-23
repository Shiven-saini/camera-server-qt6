#include "MainWindow.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "WindowsService.h"
#include "CameraDiscovery.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QSplitter>
#include <QGroupBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QProcess>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>

Q_DECLARE_METATYPE(DiscoveredCamera)

// Camera Configuration Dialog
class CameraConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraConfigDialog(const CameraConfig& camera = CameraConfig(), QWidget *parent = nullptr)
        : QDialog(parent), m_camera(camera)
    {
        setWindowTitle(camera.name().isEmpty() ? "Add Camera" : "Edit Camera");
        setModal(true);
        resize(400, 300);
        
        setupUI();
        loadCamera();
    }
    
    CameraConfig getCamera() const { return m_camera; }

private slots:
    void accept() override
    {
        saveCamera();
        if (m_camera.isValid()) {
            QDialog::accept();
        } else {
            QMessageBox::warning(this, "Invalid Configuration", 
                               "Please check all fields are correctly filled.");
        }
    }

private:    void setupUI()
    {
        QFormLayout* layout = new QFormLayout(this);
        
        m_nameEdit = new QLineEdit(this);
        m_ipEdit = new QLineEdit(this);
        m_portSpinBox = new QSpinBox(this);
        m_portSpinBox->setRange(1, 65535);
        m_portSpinBox->setValue(554);
        
        m_brandComboBox = new QComboBox(this);
        m_brandComboBox->addItems({"Generic", "Hikvision", "CP Plus", "Dahua", "Axis", "Vivotek", "Foscam"});
        m_brandComboBox->setCurrentText("Generic");
        
        m_modelEdit = new QLineEdit(this);
        
        m_usernameEdit = new QLineEdit(this);
        m_passwordEdit = new QLineEdit(this);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        
        m_enabledCheckBox = new QCheckBox(this);
        m_enabledCheckBox->setChecked(true);
        
        layout->addRow("Camera Name:", m_nameEdit);
        layout->addRow("IP Address:", m_ipEdit);
        layout->addRow("Port:", m_portSpinBox);
        layout->addRow("Brand:", m_brandComboBox);
        layout->addRow("Model:", m_modelEdit);
        layout->addRow("Username:", m_usernameEdit);
        layout->addRow("Password:", m_passwordEdit);
        layout->addRow("Enabled:", m_enabledCheckBox);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        layout->addRow(buttonBox);
    }
      void loadCamera()
    {
        m_nameEdit->setText(m_camera.name());
        m_ipEdit->setText(m_camera.ipAddress());
        m_portSpinBox->setValue(m_camera.port() > 0 ? m_camera.port() : 554);
        m_brandComboBox->setCurrentText(m_camera.brand().isEmpty() ? "Generic" : m_camera.brand());
        m_modelEdit->setText(m_camera.model());
        m_usernameEdit->setText(m_camera.username());
        m_passwordEdit->setText(m_camera.password());
        m_enabledCheckBox->setChecked(m_camera.isEnabled());
    }
    
    void saveCamera()
    {
        m_camera.setName(m_nameEdit->text().trimmed());
        m_camera.setIpAddress(m_ipEdit->text().trimmed());
        m_camera.setPort(m_portSpinBox->value());
        m_camera.setBrand(m_brandComboBox->currentText());
        m_camera.setModel(m_modelEdit->text().trimmed());
        m_camera.setUsername(m_usernameEdit->text().trimmed());
        m_camera.setPassword(m_passwordEdit->text());
        m_camera.setEnabled(m_enabledCheckBox->isChecked());
    }
      CameraConfig m_camera;
    QLineEdit* m_nameEdit;
    QLineEdit* m_ipEdit;
    QSpinBox* m_portSpinBox;
    QComboBox* m_brandComboBox;
    QLineEdit* m_modelEdit;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_enabledCheckBox;
};

// Camera Discovery Dialog
class CameraDiscoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraDiscoveryDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_discovery(nullptr)
        , m_isScanning(false)
    {
        setWindowTitle("Discover Cameras");
        setModal(true);
        resize(800, 600);
        
        setupUI();
        setupDiscovery();
    }
    
    QList<DiscoveredCamera> getSelectedCameras() const { return m_selectedCameras; }

private slots:
    void startDiscovery()
    {
        if (m_isScanning) return;
        
        m_isScanning = true;
        m_discoveredCamerasWidget->clear();
        m_selectedCameras.clear();
        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
        m_statusLabel->setText("Scanning network for cameras...");
        m_scanButton->setText("Stop Scan");
        m_scanButton->setEnabled(true);
        
        QString networkRange = m_networkEdit->text().trimmed();
        if (networkRange.isEmpty()) {
            networkRange = CameraDiscovery::detectNetworkRange();
            m_networkEdit->setText(networkRange);
        }
        
        m_discovery->startDiscovery(networkRange);
    }
    
    void stopDiscovery()
    {
        if (!m_isScanning) return;
        
        m_discovery->stopDiscovery();
        m_isScanning = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Scan stopped");
        m_scanButton->setText("Start Scan");
    }
    
    void onDiscoveryStarted()
    {
        m_isScanning = true;
        m_statusLabel->setText("Scanning network...");
    }
    
    void onDiscoveryFinished()
    {
        m_isScanning = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText(QString("Scan completed. Found %1 cameras.").arg(m_discoveredCamerasWidget->count()));
        m_scanButton->setText("Start Scan");
        m_scanButton->setEnabled(true);
    }
    
    void onDiscoveryProgress(int current, int total)
    {
        if (total > 0) {
            int percentage = (current * 100) / total;
            m_progressBar->setValue(percentage);
            m_statusLabel->setText(QString("Scanning... %1/%2 (%3%)")
                                   .arg(current).arg(total).arg(percentage));
        }
    }
    
    void onCameraDiscovered(const DiscoveredCamera& camera)
    {
        addCameraToList(camera);
    }
    
    void onSelectionChanged()
    {
        m_selectedCameras.clear();
        
        for (int i = 0; i < m_discoveredCamerasWidget->count(); ++i) {
            QListWidgetItem* item = m_discoveredCamerasWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                DiscoveredCamera camera = item->data(Qt::UserRole).value<DiscoveredCamera>();
                m_selectedCameras.append(camera);
            }
        }
        
        m_addSelectedButton->setEnabled(!m_selectedCameras.isEmpty());
        m_selectedCountLabel->setText(QString("Selected: %1").arg(m_selectedCameras.size()));
    }
    
    void onAddSelected()
    {
        accept();
    }
    
    void onItemDoubleClicked(QListWidgetItem* item)
    {
        if (item) {
            // Toggle selection on double-click
            Qt::CheckState newState = (item->checkState() == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            item->setCheckState(newState);
        }
    }

private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Network configuration
        QGroupBox* networkGroup = new QGroupBox("Network Configuration");
        QFormLayout* networkLayout = new QFormLayout(networkGroup);
        
        m_networkEdit = new QLineEdit(this);
        m_networkEdit->setText(CameraDiscovery::detectNetworkRange());
        m_networkEdit->setPlaceholderText("e.g., 192.168.1.0/24");
        networkLayout->addRow("Network Range:", m_networkEdit);
        
        mainLayout->addWidget(networkGroup);
        
        // Control buttons
        QHBoxLayout* controlLayout = new QHBoxLayout;
        m_scanButton = new QPushButton("Start Scan", this);
        connect(m_scanButton, &QPushButton::clicked, this, [this]() {
            if (m_isScanning) {
                stopDiscovery();
            } else {
                startDiscovery();
            }
        });
        controlLayout->addWidget(m_scanButton);
        
        m_statusLabel = new QLabel("Ready to scan", this);
        controlLayout->addWidget(m_statusLabel);
        controlLayout->addStretch();
        
        m_progressBar = new QProgressBar(this);
        m_progressBar->setVisible(false);
        controlLayout->addWidget(m_progressBar);
        
        mainLayout->addLayout(controlLayout);
        
        // Discovered cameras
        QGroupBox* camerasGroup = new QGroupBox("Discovered Cameras");
        QVBoxLayout* camerasLayout = new QVBoxLayout(camerasGroup);
        
        m_discoveredCamerasWidget = new QListWidget(this);
        m_discoveredCamerasWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(m_discoveredCamerasWidget, &QListWidget::itemChanged, this, &CameraDiscoveryDialog::onSelectionChanged);
        connect(m_discoveredCamerasWidget, &QListWidget::itemDoubleClicked, this, &CameraDiscoveryDialog::onItemDoubleClicked);
        camerasLayout->addWidget(m_discoveredCamerasWidget);
        
        QHBoxLayout* selectionLayout = new QHBoxLayout;
        m_selectedCountLabel = new QLabel("Selected: 0", this);
        selectionLayout->addWidget(m_selectedCountLabel);
        selectionLayout->addStretch();
        camerasLayout->addLayout(selectionLayout);
        
        mainLayout->addWidget(camerasGroup);
        
        // Dialog buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        m_addSelectedButton = new QPushButton("Add Selected Cameras", this);
        m_addSelectedButton->setEnabled(false);
        connect(m_addSelectedButton, &QPushButton::clicked, this, &CameraDiscoveryDialog::onAddSelected);
        buttonLayout->addWidget(m_addSelectedButton);
        
        QPushButton* cancelButton = new QPushButton("Cancel", this);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(cancelButton);
        
        mainLayout->addLayout(buttonLayout);
    }
    
    void setupDiscovery()
    {
        m_discovery = new CameraDiscovery(this);
        
        connect(m_discovery, &CameraDiscovery::discoveryStarted, this, &CameraDiscoveryDialog::onDiscoveryStarted);
        connect(m_discovery, &CameraDiscovery::discoveryFinished, this, &CameraDiscoveryDialog::onDiscoveryFinished);
        connect(m_discovery, &CameraDiscovery::discoveryProgress, this, &CameraDiscoveryDialog::onDiscoveryProgress);
        connect(m_discovery, &CameraDiscovery::cameraDiscovered, this, &CameraDiscoveryDialog::onCameraDiscovered);
    }
    
    void addCameraToList(const DiscoveredCamera& camera)
    {
        QListWidgetItem* item = new QListWidgetItem(m_discoveredCamerasWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        
        // Create display text with brand, IP, and model info
        QString displayText = QString("[%1] %2:%3")
                              .arg(camera.brand, camera.ipAddress).arg(camera.port);
        
        if (!camera.model.isEmpty() && camera.model != "Unknown") {
            displayText += QString(" - %1").arg(camera.model);
        }
        
        if (!camera.deviceName.isEmpty()) {
            displayText += QString(" (%1)").arg(camera.deviceName);
        }
        
        // Add RTSP URL hint
        displayText += QString("\nRTSP: %1").arg(camera.rtspUrl);
        
        item->setText(displayText);
        
        // Set icon based on brand
        if (camera.brand == "Hikvision") {
            item->setBackground(QColor(230, 250, 230)); // Light green
        } else if (camera.brand == "CP Plus") {
            item->setBackground(QColor(230, 230, 250)); // Light blue
        } else {
            item->setBackground(QColor(250, 250, 230)); // Light yellow for generic
        }
        
        // Store camera data
        item->setData(Qt::UserRole, QVariant::fromValue(camera));
        
        m_discoveredCamerasWidget->addItem(item);
    }

private:
    CameraDiscovery* m_discovery;
    bool m_isScanning;
    QList<DiscoveredCamera> m_selectedCameras;
    
    // UI elements
    QLineEdit* m_networkEdit;
    QPushButton* m_scanButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QListWidget* m_discoveredCamerasWidget;
    QLabel* m_selectedCountLabel;
    QPushButton* m_addSelectedButton;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isClosingToTray(false)
    , m_forceQuit(false)
    , m_pingProcess(nullptr)
{    setWindowTitle("Camera Server Qt6");
    setMinimumSize(800, 600);
    
    // Initialize camera manager
    LOG_INFO("Creating CameraManager...", "MainWindow");
    m_cameraManager = new CameraManager(this);
    
    LOG_INFO("Creating menu bar...", "MainWindow");
    createMenuBar();
    LOG_INFO("Creating status bar...", "MainWindow");
    createStatusBar();
    LOG_INFO("Creating central widget...", "MainWindow");
    createCentralWidget();
    LOG_INFO("Setting up connections...", "MainWindow");
    setupConnections();
      // Initialize system tray
    LOG_INFO("Creating system tray manager...", "MainWindow");
    m_trayManager = new SystemTrayManager(this, m_cameraManager, this);
    LOG_INFO("Initializing system tray...", "MainWindow");
    m_trayManager->initialize();
    
    // Setup system tray connections
    LOG_INFO("Setting up system tray connections...", "MainWindow");
    connect(m_trayManager, &SystemTrayManager::showMainWindow, [this]() {
        show();
        raise();
        activateWindow();
    });
    connect(m_trayManager, &SystemTrayManager::quitApplication, [this]() {
        m_forceQuit = true;
        close();
        qApp->quit();
    });
    
    // Load settings and initialize
    LOG_INFO("Loading settings...", "MainWindow");
    loadSettings();
    LOG_INFO("Initializing camera manager...", "MainWindow");
    m_cameraManager->initialize();
    
    LOG_INFO("Updating camera table...", "MainWindow");
    updateCameraTable();
    LOG_INFO("Updating buttons...", "MainWindow");
    updateButtons();
    
    statusBar()->showMessage("Ready", 2000);
    LOG_INFO("MainWindow initialized successfully", "MainWindow");
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::showMessage(const QString& message)
{
    statusBar()->showMessage(message, 3000);
}

void MainWindow::appendLog(const QString& message)
{
    if (m_logTextEdit) {
        m_logTextEdit->append(message);
        
        // Limit log size
        if (m_logTextEdit->document()->blockCount() > 1000) {
            QTextCursor cursor = m_logTextEdit->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
            cursor.removeSelectedText();
        }
        
        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // If the system tray is available and we're not forcing quit, minimize to tray
    if (m_trayManager && m_trayManager->isVisible() && !m_forceQuit) {
        hide();
        if (m_trayManager) {
            m_trayManager->showNotification("Camera Server Qt6", 
                "Application was minimized to tray. Right-click the tray icon for options.");
        }
        event->ignore();
        return;
    }
    
    // If we're force quitting or tray is not available, actually close
    saveSettings();
    
    // Shutdown camera manager
    if (m_cameraManager) {
        m_cameraManager->shutdown();
    }
    
    // Hide tray icon before quitting
    if (m_trayManager) {
        m_trayManager->hide();
    }
    
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && m_trayManager && m_trayManager->isVisible()) {
            // Hide to tray when minimized
            QTimer::singleShot(100, this, [this]() {
                hide();
                if (m_trayManager) {
                    m_trayManager->showNotification("Camera Server Qt6", 
                        "Application minimized to system tray");
                }
            });
            event->ignore();
            return;
        }
    }
    
    QMainWindow::changeEvent(event);
}

void MainWindow::addCamera()
{
    CameraConfigDialog dialog(CameraConfig(), this);
    if (dialog.exec() == QDialog::Accepted) {
        CameraConfig camera = dialog.getCamera();
        if (m_cameraManager->addCamera(camera)) {
            showMessage(QString("Camera '%1' added successfully").arg(camera.name()));
        } else {
            QMessageBox::warning(this, "Error", "Failed to add camera");
        }
    }
}

void MainWindow::discoverCameras()
{
    CameraDiscoveryDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QList<DiscoveredCamera> selectedCameras = dialog.getSelectedCameras();
        
        if (selectedCameras.isEmpty()) {
            showMessage("No cameras selected");
            return;
        }
        
        int addedCount = 0;
        for (const DiscoveredCamera& discoveredCamera : selectedCameras) {
            // Create CameraConfig from DiscoveredCamera
            CameraConfig camera;
            
            // Generate a name based on brand and IP
            QString cameraName = QString("%1_Camera_%2")
                                .arg(discoveredCamera.brand)
                                .arg(discoveredCamera.ipAddress.split('.').last());
            
            if (!discoveredCamera.deviceName.isEmpty() && 
                discoveredCamera.deviceName != cameraName) {
                cameraName = discoveredCamera.deviceName;
            }
            
            camera.setName(cameraName);
            camera.setIpAddress(discoveredCamera.ipAddress);
            camera.setPort(discoveredCamera.port == 80 ? 554 : discoveredCamera.port); // Default to RTSP port
            camera.setBrand(discoveredCamera.brand);
            camera.setModel(discoveredCamera.model);
            camera.setEnabled(true);
            
            // Set default credentials based on brand
            if (discoveredCamera.brand == "Hikvision") {
                camera.setUsername("admin");
                camera.setPassword("admin");
            } else if (discoveredCamera.brand == "CP Plus") {
                camera.setUsername("admin");
                camera.setPassword("admin");
            } else {
                camera.setUsername("admin");
                camera.setPassword("");
            }
            
            if (m_cameraManager->addCamera(camera)) {
                addedCount++;
                LOG_INFO(QString("Added discovered camera: %1 [%2] at %3")
                         .arg(camera.name(), camera.brand(), camera.ipAddress()), "MainWindow");
            } else {
                LOG_WARNING(QString("Failed to add discovered camera: %1 at %2")
                           .arg(cameraName, discoveredCamera.ipAddress), "MainWindow");
            }
        }
        
        showMessage(QString("Added %1 of %2 discovered cameras").arg(addedCount).arg(selectedCameras.size()));
        
        if (addedCount > 0) {
            // Show a message with RTSP URL information
            QString rtspInfo = "Discovered cameras have been added with suggested RTSP URLs:\n\n";
            for (const DiscoveredCamera& cam : selectedCameras) {
                rtspInfo += QString("• %1: %2\n").arg(cam.brand, cam.rtspUrl);
            }
            rtspInfo += "\nYou may need to adjust usernames, passwords, and RTSP paths for your specific cameras.";
            
            QMessageBox::information(this, "Camera Discovery Complete", rtspInfo);
        }
    }
}

void MainWindow::editCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    CameraConfig camera = ConfigManager::instance().getCamera(cameraId);
    
    if (camera.id().isEmpty()) {
        QMessageBox::warning(this, "Error", "Camera not found");
        return;
    }
    
    CameraConfigDialog dialog(camera, this);
    if (dialog.exec() == QDialog::Accepted) {
        CameraConfig updatedCamera = dialog.getCamera();
        if (m_cameraManager->updateCamera(cameraId, updatedCamera)) {
            showMessage(QString("Camera '%1' updated successfully").arg(updatedCamera.name()));
        } else {
            QMessageBox::warning(this, "Error", "Failed to update camera");
        }
    }
}

void MainWindow::removeCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* nameItem = m_cameraTable->item(row, 1);
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!nameItem || !idItem) return;
    
    QString cameraName = nameItem->text();
    QString cameraId = idItem->data(Qt::UserRole).toString();
    
    int ret = QMessageBox::question(this, "Confirm Removal",
                                   QString("Are you sure you want to remove camera '%1'?").arg(cameraName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        if (m_cameraManager->removeCamera(cameraId)) {
            showMessage(QString("Camera '%1' removed successfully").arg(cameraName));
        } else {
            QMessageBox::warning(this, "Error", "Failed to remove camera");
        }
    }
}

void MainWindow::toggleCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    
    if (m_cameraManager->isCameraRunning(cameraId)) {
        m_cameraManager->stopCamera(cameraId);
    } else {
        m_cameraManager->startCamera(cameraId);
    }
}

void MainWindow::startAllCameras()
{
    m_cameraManager->startAllCameras();
    showMessage("Starting all enabled cameras...");
}

void MainWindow::stopAllCameras()
{
    m_cameraManager->stopAllCameras();
    showMessage("Stopping all cameras...");
}

void MainWindow::toggleAutoStart()
{
    bool enabled = m_autoStartCheckBox->isChecked();
    ConfigManager::instance().setAutoStartEnabled(enabled);
    showMessage(QString("Auto-start %1").arg(enabled ? "enabled" : "disabled"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Camera Server Qt6",
                      "Camera Server Qt6\n\n"
                      "IP Camera Port Forwarding Application\n"
                      "Built with Qt 6.5.3\n\n"
                      "This application provides port forwarding for IP cameras\n"
                      "across VPN connections with P2P connectivity.");
}

void MainWindow::onCameraSelectionChanged()
{
    updateButtons();
}

void MainWindow::onCameraStarted(const QString& id)
{
    updateCameraTable();
    updateButtons();
    
    CameraConfig camera = ConfigManager::instance().getCamera(id);
    showMessage(QString("Camera '%1' started").arg(camera.name()));
    
    if (m_trayManager) {
        m_trayManager->updateCameraStatus();
        m_trayManager->notifyCameraStatusChange(camera.name(), true);
    }
}

void MainWindow::onCameraStopped(const QString& id)
{
    updateCameraTable();
    updateButtons();
    
    CameraConfig camera = ConfigManager::instance().getCamera(id);
    showMessage(QString("Camera '%1' stopped").arg(camera.name()));
    
    if (m_trayManager) {
        m_trayManager->updateCameraStatus();
        m_trayManager->notifyCameraStatusChange(camera.name(), false);
    }
}

void MainWindow::onCameraError(const QString& id, const QString& error)
{
    CameraConfig camera = ConfigManager::instance().getCamera(id);
    QString message = QString("Camera '%1' error: %2").arg(camera.name()).arg(error);
    showMessage(message);
    
    LOG_ERROR(message, "MainWindow");
}

void MainWindow::onConfigurationChanged()
{
    updateCameraTable();
    updateButtons();
}

void MainWindow::onLogMessage(const QString& message)
{
    appendLog(message);
}

void MainWindow::createMenuBar()
{
    // File menu
    m_fileMenu = menuBar()->addMenu("&File");
    
    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    m_fileMenu->addAction(m_exitAction);
    
    // Service menu
    m_serviceMenu = menuBar()->addMenu("&Service");
    
    m_installServiceAction = new QAction("&Install Service", this);
    connect(m_installServiceAction, &QAction::triggered, [this]() {
        if (WindowsService::instance().installService()) {
            QMessageBox::information(this, "Success", "Service installed successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to install service");
        }
    });
    m_serviceMenu->addAction(m_installServiceAction);
    
    m_uninstallServiceAction = new QAction("&Uninstall Service", this);
    connect(m_uninstallServiceAction, &QAction::triggered, [this]() {
        if (WindowsService::instance().uninstallService()) {
            QMessageBox::information(this, "Success", "Service uninstalled successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to uninstall service");
        }
    });
    m_serviceMenu->addAction(m_uninstallServiceAction);
    
    // Help menu
    m_helpMenu = menuBar()->addMenu("&Help");
    
    m_aboutAction = new QAction("&About", this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("Ready");
}

void MainWindow::createCentralWidget()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Vertical, m_centralWidget);
    
    // Camera management group
    m_cameraGroupBox = new QGroupBox("Camera Configuration");
    QVBoxLayout* cameraLayout = new QVBoxLayout(m_cameraGroupBox);      // Camera table
    m_cameraTable = new QTableWidget(0, 9);
    QStringList headers = {"#", "Name", "Brand", "Model", "IP Address", "Port", "External Port", "Status", "Test"};
    m_cameraTable->setHorizontalHeaderLabels(headers);
    m_cameraTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cameraTable->setAlternatingRowColors(true);
    m_cameraTable->horizontalHeader()->setStretchLastSection(true);
    
    cameraLayout->addWidget(m_cameraTable);      // Camera buttons
    QHBoxLayout* cameraButtonLayout = new QHBoxLayout;
    m_addButton = new QPushButton("Add Camera");
    m_discoverButton = new QPushButton("Discover Cameras");
    m_editButton = new QPushButton("Edit Camera");
    m_removeButton = new QPushButton("Remove Camera");
    m_toggleButton = new QPushButton("Start/Stop");
    m_testButton = new QPushButton("Test Camera");
    
    cameraButtonLayout->addWidget(m_addButton);
    cameraButtonLayout->addWidget(m_discoverButton);
    cameraButtonLayout->addWidget(m_editButton);
    cameraButtonLayout->addWidget(m_removeButton);
    cameraButtonLayout->addWidget(m_toggleButton);
    cameraButtonLayout->addWidget(m_testButton);
    cameraButtonLayout->addStretch();
    
    cameraLayout->addLayout(cameraButtonLayout);
    
    // Service control group
    m_serviceGroupBox = new QGroupBox("Service Control");
    QVBoxLayout* serviceLayout = new QVBoxLayout(m_serviceGroupBox);
    
    QHBoxLayout* serviceButtonLayout = new QHBoxLayout;
    m_startAllButton = new QPushButton("Start All Cameras");
    m_stopAllButton = new QPushButton("Stop All Cameras");
    
    serviceButtonLayout->addWidget(m_startAllButton);
    serviceButtonLayout->addWidget(m_stopAllButton);
    serviceButtonLayout->addStretch();
    
    serviceLayout->addLayout(serviceButtonLayout);
    
    // Auto-start checkbox
    m_autoStartCheckBox = new QCheckBox("Auto-start with Windows");
    serviceLayout->addWidget(m_autoStartCheckBox);
    
    // Service status
    m_serviceStatusLabel = new QLabel("Service Status: Ready");
    serviceLayout->addWidget(m_serviceStatusLabel);
    
    // Log viewer group
    m_logGroupBox = new QGroupBox("Application Log");
    QVBoxLayout* logLayout = new QVBoxLayout(m_logGroupBox);
    
    m_logTextEdit = new QTextEdit;
    m_logTextEdit->setMaximumHeight(200);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    
    logLayout->addWidget(m_logTextEdit);
    
    QHBoxLayout* logButtonLayout = new QHBoxLayout;
    m_clearLogButton = new QPushButton("Clear Log");
    connect(m_clearLogButton, &QPushButton::clicked, m_logTextEdit, &QTextEdit::clear);
    
    logButtonLayout->addWidget(m_clearLogButton);
    logButtonLayout->addStretch();
    
    logLayout->addLayout(logButtonLayout);
    
    // Add to splitter
    QWidget* topWidget = new QWidget;
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->addWidget(m_cameraGroupBox);
    topLayout->addWidget(m_serviceGroupBox);
    
    m_mainSplitter->addWidget(topWidget);
    m_mainSplitter->addWidget(m_logGroupBox);
    m_mainSplitter->setSizes({400, 200});
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_mainSplitter);
}

void MainWindow::setupConnections()
{
    // Camera table
    connect(m_cameraTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onCameraSelectionChanged);
    connect(m_cameraTable, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::editCamera);      // Camera buttons
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addCamera);
    connect(m_discoverButton, &QPushButton::clicked, this, &MainWindow::discoverCameras);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editCamera);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::removeCamera);
    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(m_testButton, &QPushButton::clicked, this, &MainWindow::testCamera);
    
    // Service buttons
    connect(m_startAllButton, &QPushButton::clicked, this, &MainWindow::startAllCameras);
    connect(m_stopAllButton, &QPushButton::clicked, this, &MainWindow::stopAllCameras);
    connect(m_autoStartCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleAutoStart);
    
    // Camera manager
    connect(m_cameraManager, &CameraManager::cameraStarted,
            this, &MainWindow::onCameraStarted);
    connect(m_cameraManager, &CameraManager::cameraStopped,
            this, &MainWindow::onCameraStopped);
    connect(m_cameraManager, &CameraManager::cameraError,
            this, &MainWindow::onCameraError);
    connect(m_cameraManager, &CameraManager::configurationChanged,
            this, &MainWindow::onConfigurationChanged);
      // Logger
    connect(&Logger::instance(), &Logger::logMessage,
            this, &MainWindow::onLogMessage);
}

void MainWindow::updateCameraTable()
{
    m_cameraTable->setRowCount(0);
    
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    for (int i = 0; i < cameras.size(); ++i) {
        const CameraConfig& camera = cameras[i];
        
        m_cameraTable->insertRow(i);
        
        // Index (hidden, stores camera ID)
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setData(Qt::UserRole, camera.id());
        m_cameraTable->setItem(i, 0, indexItem);
        
        // Name
        m_cameraTable->setItem(i, 1, new QTableWidgetItem(camera.name()));
        
        // Brand
        QTableWidgetItem* brandItem = new QTableWidgetItem(camera.brand());
        if (camera.brand() == "Hikvision") {
            brandItem->setBackground(QColor(230, 250, 230)); // Light green
        } else if (camera.brand() == "CP Plus") {
            brandItem->setBackground(QColor(230, 230, 250)); // Light blue
        } else if (camera.brand() == "Generic") {
            brandItem->setBackground(QColor(250, 250, 230)); // Light yellow
        }
        m_cameraTable->setItem(i, 2, brandItem);
        
        // Model
        m_cameraTable->setItem(i, 3, new QTableWidgetItem(camera.model().isEmpty() ? "Unknown" : camera.model()));
        
        // IP Address
        m_cameraTable->setItem(i, 4, new QTableWidgetItem(camera.ipAddress()));
        
        // Port
        m_cameraTable->setItem(i, 5, new QTableWidgetItem(QString::number(camera.port())));
        
        // External Port
        m_cameraTable->setItem(i, 6, new QTableWidgetItem(QString::number(camera.externalPort())));
        
        // Status
        bool isRunning = m_cameraManager->isCameraRunning(camera.id());
        QString status;
        if (!camera.isEnabled()) {
            status = "Disabled";
        } else if (isRunning) {
            status = "Running";
        } else {
            status = "Stopped";
        }
        
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (isRunning) {
            statusItem->setBackground(QColor(144, 238, 144)); // Light green
        } else if (!camera.isEnabled()) {
            statusItem->setBackground(QColor(211, 211, 211)); // Light gray
        } else {
            statusItem->setBackground(QColor(255, 182, 193)); // Light red
        }
        m_cameraTable->setItem(i, 7, statusItem);
        
        // Test column - shows connectivity status
        QTableWidgetItem* testItem = new QTableWidgetItem("Click Test");
        testItem->setTextAlignment(Qt::AlignCenter);
        testItem->setBackground(QColor(240, 240, 240)); // Light gray
        m_cameraTable->setItem(i, 8, testItem);
    }
    
    // Resize columns to content
    m_cameraTable->resizeColumnsToContents();
}

void MainWindow::updateButtons()
{
    bool hasSelection = m_cameraTable->currentRow() >= 0;
    bool hasCamera = m_cameraTable->rowCount() > 0;
      m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_toggleButton->setEnabled(hasSelection);
    m_testButton->setEnabled(hasSelection);
    
    m_startAllButton->setEnabled(hasCamera);
    m_stopAllButton->setEnabled(hasCamera);
    
    // Update toggle button text
    if (hasSelection) {
        QTableWidgetItem* idItem = m_cameraTable->item(m_cameraTable->currentRow(), 0);
        if (idItem) {
            QString cameraId = idItem->data(Qt::UserRole).toString();
            bool isRunning = m_cameraManager->isCameraRunning(cameraId);
            m_toggleButton->setText(isRunning ? "Stop Camera" : "Start Camera");
        }
    } else {
        m_toggleButton->setText("Start/Stop");
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    
    // Window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Splitter state
    if (m_mainSplitter) {
        m_mainSplitter->restoreState(settings.value("splitterState").toByteArray());
    }
    
    // Auto-start setting
    m_autoStartCheckBox->setChecked(ConfigManager::instance().isAutoStartEnabled());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    
    // Window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    // Splitter state
    if (m_mainSplitter) {
        settings.setValue("splitterState", m_mainSplitter->saveState());
    }
}

void MainWindow::testCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    QTableWidgetItem* ipItem = m_cameraTable->item(row, 4);  // IP Address column
    QTableWidgetItem* testItem = m_cameraTable->item(row, 8); // Test column
    
    if (!idItem || !ipItem || !testItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    QString ipAddress = ipItem->text();
    
    // Clean up previous ping process
    if (m_pingProcess) {
        m_pingProcess->kill();
        m_pingProcess->deleteLater();
    }
    
    // Update UI to show testing state
    testItem->setText("Testing...");
    testItem->setBackground(QColor(255, 255, 0)); // Yellow
    m_testButton->setEnabled(false);
    
    // Store current testing camera
    m_currentTestingCameraId = cameraId;
    
    // Create new ping process
    m_pingProcess = new QProcess(this);
    connect(m_pingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onPingFinished);
    
    // Start ping command (Windows)
    QStringList arguments;
    arguments << "-n" << "3" << "-w" << "3000" << ipAddress; // 3 pings, 3 second timeout
    
    LOG_INFO(QString("Testing camera '%1' at IP: %2").arg(cameraId, ipAddress), "MainWindow");
    showMessage(QString("Testing camera at %1...").arg(ipAddress));
    
    m_pingProcess->start("ping", arguments);
    
    // Set a timeout timer in case ping hangs
    QTimer::singleShot(15000, this, [this]() {
        if (m_pingProcess && m_pingProcess->state() == QProcess::Running) {
            m_pingProcess->kill();
            onPingFinished(-1, QProcess::CrashExit);
        }
    });
}

void MainWindow::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable test button
    m_testButton->setEnabled(true);
    
    // Find the row for the current testing camera
    int testRow = -1;
    for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
        QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
        if (idItem && idItem->data(Qt::UserRole).toString() == m_currentTestingCameraId) {
            testRow = i;
            break;
        }
    }
      if (testRow >= 0) {
        QTableWidgetItem* testItem = m_cameraTable->item(testRow, 8); // Test column
        QTableWidgetItem* ipItem = m_cameraTable->item(testRow, 4);   // IP Address column
        
        if (testItem && ipItem) {
            QString ipAddress = ipItem->text();
            
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                // Ping successful
                testItem->setText("✓ Online");
                testItem->setBackground(QColor(144, 238, 144)); // Light green
                showMessage(QString("Camera at %1 is online and reachable").arg(ipAddress));
                LOG_INFO(QString("Ping test successful for camera at %1").arg(ipAddress), "MainWindow");
            } else {
                // Ping failed
                testItem->setText("✗ Offline");
                testItem->setBackground(QColor(255, 182, 193)); // Light red
                showMessage(QString("Camera at %1 is not reachable").arg(ipAddress));
                LOG_WARNING(QString("Ping test failed for camera at %1 (exit code: %2)").arg(ipAddress).arg(exitCode), "MainWindow");
            }
        }
    }
    
    // Clean up
    if (m_pingProcess) {
        m_pingProcess->deleteLater();
        m_pingProcess = nullptr;
    }
    
    m_currentTestingCameraId.clear();
}

#include "MainWindow.moc" // Include MOC file for Q_OBJECT in CameraConfigDialog
