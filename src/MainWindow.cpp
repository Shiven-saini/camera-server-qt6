#include "MainWindow.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "WindowsService.h"
#include "CameraDiscovery.h"
#include "VpnWidget.h"
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
#include <QClipboard>

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
        
        // Credentials section with group box for better organization
        QGroupBox* credentialsGroup = new QGroupBox("Camera Credentials", this);
        QFormLayout* credentialsLayout = new QFormLayout(credentialsGroup);
        
        m_usernameEdit = new QLineEdit(this);
        m_usernameEdit->setPlaceholderText("Enter camera username (e.g., admin)");
        
        // Password field with visibility toggle
        QWidget* passwordWidget = new QWidget(this);
        QHBoxLayout* passwordLayout = new QHBoxLayout(passwordWidget);
        passwordLayout->setContentsMargins(0, 0, 0, 0);
        
        m_passwordEdit = new QLineEdit(this);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("Enter camera password");
          m_passwordVisibilityButton = new QPushButton(this);
        m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        m_passwordVisibilityButton->setToolTip("Show/Hide Password");
        m_passwordVisibilityButton->setMaximumWidth(30);
        m_passwordVisibilityButton->setFlat(true);
        
        connect(m_passwordVisibilityButton, &QPushButton::clicked, this, &CameraConfigDialog::togglePasswordVisibility);
        
        passwordLayout->addWidget(m_passwordEdit);
        passwordLayout->addWidget(m_passwordVisibilityButton);
        
        credentialsLayout->addRow("Username:", m_usernameEdit);
        credentialsLayout->addRow("Password:", passwordWidget);
        
        // Add credential presets button
        m_credentialPresetsButton = new QPushButton("Load Common Credentials", this);
        connect(m_credentialPresetsButton, &QPushButton::clicked, this, &CameraConfigDialog::showCredentialPresets);
        credentialsLayout->addRow("", m_credentialPresetsButton);
        
        m_enabledCheckBox = new QCheckBox(this);
        m_enabledCheckBox->setChecked(true);
        
        layout->addRow("Camera Name:", m_nameEdit);
        layout->addRow("IP Address:", m_ipEdit);
        layout->addRow("Port:", m_portSpinBox);
        layout->addRow("Brand:", m_brandComboBox);
        layout->addRow("Model:", m_modelEdit);
        layout->addRow(credentialsGroup);
        layout->addRow("Enabled:", m_enabledCheckBox);
        
        // RTSP URL preview
        m_rtspPreviewGroup = new QGroupBox("RTSP URL Preview", this);
        QVBoxLayout* rtspLayout = new QVBoxLayout(m_rtspPreviewGroup);
        
        m_rtspUrlLabel = new QLabel("rtsp://username:password@192.168.1.100:554/stream", this);
        m_rtspUrlLabel->setWordWrap(true);
        m_rtspUrlLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; border: 1px solid #ccc; }");
        m_rtspUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        
        QPushButton* copyUrlButton = new QPushButton("Copy to Clipboard", this);
        connect(copyUrlButton, &QPushButton::clicked, this, &CameraConfigDialog::copyRtspUrl);
        
        rtspLayout->addWidget(m_rtspUrlLabel);
        rtspLayout->addWidget(copyUrlButton);
        
        layout->addRow(m_rtspPreviewGroup);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        layout->addRow(buttonBox);
        
        // Connect signals to update RTSP preview
        connect(m_usernameEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_passwordEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_ipEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &CameraConfigDialog::updateRtspPreview);
        connect(m_brandComboBox, &QComboBox::currentTextChanged, this, &CameraConfigDialog::updateRtspPreview);
    }    void loadCamera()
    {
        m_nameEdit->setText(m_camera.name());
        m_ipEdit->setText(m_camera.ipAddress());
        m_portSpinBox->setValue(m_camera.port() > 0 ? m_camera.port() : 554);
        m_brandComboBox->setCurrentText(m_camera.brand().isEmpty() ? "Generic" : m_camera.brand());
        m_modelEdit->setText(m_camera.model());
        m_usernameEdit->setText(m_camera.username());
        m_passwordEdit->setText(m_camera.password());
        m_enabledCheckBox->setChecked(m_camera.isEnabled());
        
        // Update RTSP preview after loading
        updateRtspPreview();
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
    }    CameraConfig m_camera;
    QLineEdit* m_nameEdit;
    QLineEdit* m_ipEdit;
    QSpinBox* m_portSpinBox;
    QComboBox* m_brandComboBox;
    QLineEdit* m_modelEdit;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_enabledCheckBox;
    
    // UI enhancement elements
    QPushButton* m_passwordVisibilityButton;
    QPushButton* m_credentialPresetsButton;
    QGroupBox* m_rtspPreviewGroup;
    QLabel* m_rtspUrlLabel;
    
private slots:
    void togglePasswordVisibility()
    {
        if (m_passwordEdit->echoMode() == QLineEdit::Password) {
            m_passwordEdit->setEchoMode(QLineEdit::Normal);
            m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
            m_passwordVisibilityButton->setToolTip("Hide Password");
        } else {
            m_passwordEdit->setEchoMode(QLineEdit::Password);
            m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            m_passwordVisibilityButton->setToolTip("Show Password");
        }
    }
    
    void showCredentialPresets()
    {
        QDialog presetDialog(this);
        presetDialog.setWindowTitle("Common Camera Credentials");
        presetDialog.setModal(true);
        presetDialog.resize(400, 300);
        
        QVBoxLayout* layout = new QVBoxLayout(&presetDialog);
        
        QLabel* infoLabel = new QLabel("Select common camera credentials based on brand:", &presetDialog);
        layout->addWidget(infoLabel);
        
        QListWidget* presetList = new QListWidget(&presetDialog);
        
        // Add common presets
        QListWidgetItem* hikvisionItem = new QListWidgetItem("Hikvision: admin / admin");
        hikvisionItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(hikvisionItem);
        
        QListWidgetItem* cpplusItem = new QListWidgetItem("CP Plus: admin / admin");
        cpplusItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(cpplusItem);
        
        QListWidgetItem* dahuaItem = new QListWidgetItem("Dahua: admin / admin");
        dahuaItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(dahuaItem);
        
        QListWidgetItem* axisItem = new QListWidgetItem("Axis: root / pass");
        axisItem->setData(Qt::UserRole, QStringList() << "root" << "pass");
        presetList->addItem(axisItem);
        
        QListWidgetItem* foscamItem = new QListWidgetItem("Foscam: admin / (empty)");
        foscamItem->setData(Qt::UserRole, QStringList() << "admin" << "");
        presetList->addItem(foscamItem);
        
        QListWidgetItem* genericItem = new QListWidgetItem("Generic: admin / password");
        genericItem->setData(Qt::UserRole, QStringList() << "admin" << "password");
        presetList->addItem(genericItem);
        
        QListWidgetItem* emptyItem = new QListWidgetItem("No Authentication (empty credentials)");
        emptyItem->setData(Qt::UserRole, QStringList() << "" << "");
        presetList->addItem(emptyItem);
        
        layout->addWidget(presetList);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &presetDialog);
        layout->addWidget(buttonBox);
        
        connect(buttonBox, &QDialogButtonBox::accepted, &presetDialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &presetDialog, &QDialog::reject);
        
        connect(presetList, &QListWidget::itemDoubleClicked, [&](QListWidgetItem* item) {
            QStringList credentials = item->data(Qt::UserRole).toStringList();
            if (credentials.size() >= 2) {
                m_usernameEdit->setText(credentials[0]);
                m_passwordEdit->setText(credentials[1]);
                presetDialog.accept();
            }
        });
        
        if (presetDialog.exec() == QDialog::Accepted) {
            QListWidgetItem* currentItem = presetList->currentItem();
            if (currentItem) {
                QStringList credentials = currentItem->data(Qt::UserRole).toStringList();
                if (credentials.size() >= 2) {
                    m_usernameEdit->setText(credentials[0]);
                    m_passwordEdit->setText(credentials[1]);
                    updateRtspPreview();
                }
            }
        }
    }
    
    void updateRtspPreview()
    {
        QString username = m_usernameEdit->text().trimmed();
        QString password = m_passwordEdit->text();
        QString ipAddress = m_ipEdit->text().trimmed();
        int port = m_portSpinBox->value();
        QString brand = m_brandComboBox->currentText();
        
        if (ipAddress.isEmpty()) {
            ipAddress = "192.168.1.100"; // Default placeholder
        }
        
        // Generate brand-specific RTSP path
        QString rtspPath = "/stream1"; // Default
        if (brand == "Hikvision") {
            rtspPath = "/Streaming/Channels/101";
        } else if (brand == "CP Plus") {
            rtspPath = "/cam/realmonitor?channel=1&subtype=0";
        } else if (brand == "Dahua") {
            rtspPath = "/cam/realmonitor?channel=1&subtype=0";
        } else if (brand == "Axis") {
            rtspPath = "/axis-media/media.amp";
        } else if (brand == "Vivotek") {
            rtspPath = "/live.sdp";
        } else if (brand == "Foscam") {
            rtspPath = "/videoMain";
        }
        
        // Build RTSP URL with proper credentials handling
        QString rtspUrl;
        if (!username.isEmpty() || !password.isEmpty()) {
            if (!username.isEmpty() && !password.isEmpty()) {
                rtspUrl = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(rtspPath);
            } else if (!username.isEmpty()) {
                rtspUrl = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(rtspPath);
            } else {
                // Only password (unusual but handled)
                rtspUrl = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(rtspPath);
            }
        } else {
            // No credentials
            rtspUrl = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(rtspPath);
        }
        
        m_rtspUrlLabel->setText(rtspUrl);
        
        // Update tooltip with additional format examples
        QString tooltip = QString("RTSP URL for %1 camera\n\nCommon formats for %1:\n").arg(brand);
        
        if (brand == "Hikvision") {
            tooltip += "• /Streaming/Channels/101 (Main stream)\n";
            tooltip += "• /Streaming/Channels/102 (Sub stream)\n";
            tooltip += "• /h264_stream";
        } else if (brand == "CP Plus") {
            tooltip += "• /cam/realmonitor?channel=1&subtype=0 (Main)\n";
            tooltip += "• /cam/realmonitor?channel=1&subtype=1 (Sub)\n";
            tooltip += "• /streaming/channels/1";
        } else if (brand == "Dahua") {
            tooltip += "• /cam/realmonitor?channel=1&subtype=0\n";
            tooltip += "• /streaming/channels/1";
        } else if (brand == "Axis") {
            tooltip += "• /axis-media/media.amp\n";
            tooltip += "• /mjpg/video.mjpg";
        } else {
            tooltip += "• /stream1\n• /live\n• /video1";
        }
        
        m_rtspUrlLabel->setToolTip(tooltip);
    }
    
    void copyRtspUrl()
    {
        QApplication::clipboard()->setText(m_rtspUrlLabel->text());
        QMessageBox::information(this, "Copied", "RTSP URL copied to clipboard!");    }
};

// Camera Information Dialog
class CameraInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraInfoDialog(const CameraConfig& camera, QWidget *parent = nullptr)
        : QDialog(parent), m_camera(camera)
    {
        setWindowTitle(QString("Camera Information - %1").arg(camera.name()));
        setModal(true);
        resize(600, 500);
        
        setupUI();
        updateRtspInfo();
    }

private slots:
    void copyMainRtspUrl()
    {
        QApplication::clipboard()->setText(m_mainRtspLabel->text());
        showCopyMessage("Main RTSP URL copied to clipboard!");
    }
    
    void copyExternalRtspUrl()
    {
        QApplication::clipboard()->setText(m_externalRtspLabel->text());
        showCopyMessage("External RTSP URL copied to clipboard!");
    }
    
    void copyAlternativeUrl()
    {
        QListWidgetItem* currentItem = m_alternativeUrlsList->currentItem();
        if (currentItem) {
            QApplication::clipboard()->setText(currentItem->text());
            showCopyMessage("Alternative RTSP URL copied to clipboard!");
        }
    }
    
    void editCamera()
    {
        accept();
        QTimer::singleShot(0, parent(), [this]() {
            if (MainWindow* mainWindow = qobject_cast<MainWindow*>(parent())) {
                mainWindow->editCamera();
            }
        });
    }
    
    void testConnection()
    {
        accept();
        QTimer::singleShot(0, parent(), [this]() {
            if (MainWindow* mainWindow = qobject_cast<MainWindow*>(parent())) {
                mainWindow->testCamera();
            }
        });
    }

private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Camera details group
        QGroupBox* detailsGroup = new QGroupBox("Camera Details", this);
        QFormLayout* detailsLayout = new QFormLayout(detailsGroup);
        
        detailsLayout->addRow("Name:", new QLabel(m_camera.name()));
        detailsLayout->addRow("Brand:", new QLabel(m_camera.brand().isEmpty() ? "Generic" : m_camera.brand()));
        detailsLayout->addRow("Model:", new QLabel(m_camera.model().isEmpty() ? "Unknown" : m_camera.model()));
        detailsLayout->addRow("IP Address:", new QLabel(m_camera.ipAddress()));
        detailsLayout->addRow("Port:", new QLabel(QString::number(m_camera.port())));
        detailsLayout->addRow("External Port:", new QLabel(QString::number(m_camera.externalPort())));
        
        // Credentials info with privacy
        QString credentialInfo = generateCredentialInfo();
        detailsLayout->addRow("Credentials:", new QLabel(credentialInfo));
        
        detailsLayout->addRow("Status:", new QLabel(m_camera.isEnabled() ? "Enabled" : "Disabled"));
        
        mainLayout->addWidget(detailsGroup);
        
        // RTSP URLs group
        QGroupBox* rtspGroup = new QGroupBox("RTSP URLs", this);
        QVBoxLayout* rtspLayout = new QVBoxLayout(rtspGroup);
        
        // Main RTSP URL (local network)
        QHBoxLayout* mainRtspLayout = new QHBoxLayout;
        QLabel* mainLabel = new QLabel("Local Network URL:", this);
        mainLabel->setStyleSheet("font-weight: bold;");
        mainRtspLayout->addWidget(mainLabel);
        
        m_mainRtspLabel = new QLabel(this);
        m_mainRtspLabel->setWordWrap(true);
        m_mainRtspLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_mainRtspLabel->setStyleSheet("QLabel { background-color: #f0f8ff; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; }");
        
        QPushButton* copyMainBtn = new QPushButton("Copy", this);
        copyMainBtn->setMaximumWidth(60);
        connect(copyMainBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyMainRtspUrl);
        
        QHBoxLayout* mainUrlLayout = new QHBoxLayout;
        mainUrlLayout->addWidget(m_mainRtspLabel, 1);
        mainUrlLayout->addWidget(copyMainBtn);
        
        rtspLayout->addLayout(mainRtspLayout);
        rtspLayout->addLayout(mainUrlLayout);
        
        // External RTSP URL (for external access)
        QHBoxLayout* externalRtspLayout = new QHBoxLayout;
        QLabel* externalLabel = new QLabel("External Access URL:", this);
        externalLabel->setStyleSheet("font-weight: bold;");
        externalRtspLayout->addWidget(externalLabel);
        
        m_externalRtspLabel = new QLabel(this);
        m_externalRtspLabel->setWordWrap(true);
        m_externalRtspLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_externalRtspLabel->setStyleSheet("QLabel { background-color: #f0fff0; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; }");
        
        QPushButton* copyExternalBtn = new QPushButton("Copy", this);
        copyExternalBtn->setMaximumWidth(60);
        connect(copyExternalBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyExternalRtspUrl);
        
        QHBoxLayout* externalUrlLayout = new QHBoxLayout;
        externalUrlLayout->addWidget(m_externalRtspLabel, 1);
        externalUrlLayout->addWidget(copyExternalBtn);
        
        rtspLayout->addLayout(externalRtspLayout);
        rtspLayout->addLayout(externalUrlLayout);
        
        // Alternative RTSP paths
        QLabel* altLabel = new QLabel("Alternative RTSP Paths:", this);
        altLabel->setStyleSheet("font-weight: bold;");
        rtspLayout->addWidget(altLabel);
        
        m_alternativeUrlsList = new QListWidget(this);
        m_alternativeUrlsList->setMaximumHeight(120);
        connect(m_alternativeUrlsList, &QListWidget::itemDoubleClicked, this, &CameraInfoDialog::copyAlternativeUrl);
        rtspLayout->addWidget(m_alternativeUrlsList);
        
        QPushButton* copyAltBtn = new QPushButton("Copy Selected Alternative", this);
        connect(copyAltBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyAlternativeUrl);
        rtspLayout->addWidget(copyAltBtn);
        
        mainLayout->addWidget(rtspGroup);
        
        // Action buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        
        QPushButton* editBtn = new QPushButton("Edit Camera", this);
        editBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        connect(editBtn, &QPushButton::clicked, this, &CameraInfoDialog::editCamera);
        buttonLayout->addWidget(editBtn);
        
        QPushButton* testBtn = new QPushButton("Test Connection", this);
        testBtn->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
        connect(testBtn, &QPushButton::clicked, this, &CameraInfoDialog::testConnection);
        buttonLayout->addWidget(testBtn);
        
        buttonLayout->addStretch();
        
        QPushButton* closeBtn = new QPushButton("Close", this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(closeBtn);
        
        mainLayout->addLayout(buttonLayout);
    }
    
    void updateRtspInfo()
    {
        QString username = m_camera.username();
        QString password = m_camera.password();
        QString ipAddress = m_camera.ipAddress();
        int port = m_camera.port();
        int externalPort = m_camera.externalPort();
        QString brand = m_camera.brand();
        
        // Generate brand-specific RTSP path
        QString rtspPath = "/stream1"; // Default
        if (brand == "Hikvision") {
            rtspPath = "/Streaming/Channels/101";
        } else if (brand == "CP Plus") {
            rtspPath = "/cam/realmonitor?channel=1&subtype=0";
        } else if (brand == "Dahua") {
            rtspPath = "/cam/realmonitor?channel=1&subtype=0";
        } else if (brand == "Axis") {
            rtspPath = "/axis-media/media.amp";
        } else if (brand == "Vivotek") {
            rtspPath = "/live.sdp";
        } else if (brand == "Foscam") {
            rtspPath = "/videoMain";
        }
        
        // Build RTSP URLs with proper credentials handling
        QString baseLocalUrl = QString("rtsp://%1:%2").arg(ipAddress).arg(port);
        QString baseExternalUrl = QString("rtsp://[EXTERNAL_IP]:%1").arg(externalPort);
        
        QString localRtspUrl, externalRtspUrl;
        
        if (!username.isEmpty() || !password.isEmpty()) {
            if (!username.isEmpty() && !password.isEmpty()) {
                localRtspUrl = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://%1:%2@[EXTERNAL_IP]:%3%4").arg(username, password).arg(externalPort).arg(rtspPath);
            } else if (!username.isEmpty()) {
                localRtspUrl = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://%1@[EXTERNAL_IP]:%2%3").arg(username).arg(externalPort).arg(rtspPath);
            } else {
                // Only password (unusual but handled)
                localRtspUrl = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://:%1@[EXTERNAL_IP]:%2%3").arg(password).arg(externalPort).arg(rtspPath);
            }
        } else {
            // No credentials
            localRtspUrl = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(rtspPath);
            externalRtspUrl = QString("rtsp://[EXTERNAL_IP]:%1%2").arg(externalPort).arg(rtspPath);
        }
        
        m_mainRtspLabel->setText(localRtspUrl);
        m_externalRtspLabel->setText(externalRtspUrl);
        
        // Populate alternative RTSP paths
        populateAlternativePaths(username, password, ipAddress, port, brand);
    }
    
    void populateAlternativePaths(const QString& username, const QString& password, 
                                  const QString& ipAddress, int port, const QString& brand)
    {
        m_alternativeUrlsList->clear();
        
        QStringList alternativePaths;
        
        if (brand == "Hikvision") {
            alternativePaths << "/Streaming/Channels/102" << "/h264_stream" << "/ch1/main/av_stream";
        } else if (brand == "CP Plus") {
            alternativePaths << "/cam/realmonitor?channel=1&subtype=1" << "/streaming/channels/1" << "/stream1";
        } else if (brand == "Dahua") {
            alternativePaths << "/cam/realmonitor?channel=1&subtype=1" << "/streaming/channels/1" << "/stream1";
        } else if (brand == "Axis") {
            alternativePaths << "/mjpg/video.mjpg" << "/axis-media/media.amp?resolution=640x480";
        } else if (brand == "Foscam") {
            alternativePaths << "/videoSub" << "/mjpeg_stream";
        } else {
            alternativePaths << "/live" << "/video1" << "/cam1" << "/h264" << "/mjpeg";
        }
        
        for (const QString& path : alternativePaths) {
            QString url;
            if (!username.isEmpty() && !password.isEmpty()) {
                url = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(path);
            } else if (!username.isEmpty()) {
                url = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(path);
            } else if (!password.isEmpty()) {
                url = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(path);
            } else {
                url = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(path);
            }
            
            QListWidgetItem* item = new QListWidgetItem(url);
            item->setToolTip("Double-click to copy this URL");
            m_alternativeUrlsList->addItem(item);
        }
    }
    
    QString generateCredentialInfo()
    {
        QString username = m_camera.username();
        QString password = m_camera.password();
        
        if (username.isEmpty() && password.isEmpty()) {
            return "No authentication";
        } else if (!username.isEmpty() && !password.isEmpty()) {
            return QString("Username: %1, Password: %2").arg(username, QString("*").repeated(password.length()));
        } else if (!username.isEmpty()) {
            return QString("Username: %1, No password").arg(username);
        } else {
            return QString("No username, Password: %1").arg(QString("*").repeated(password.length()));
        }
    }
    
    void showCopyMessage(const QString& message)
    {
        QLabel* statusLabel = new QLabel(message, this);
        statusLabel->setStyleSheet("QLabel { background-color: #d4edda; color: #155724; padding: 5px; border: 1px solid #c3e6cb; border-radius: 4px; }");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setAttribute(Qt::WA_DeleteOnClose);
        
        // Position at bottom of dialog temporarily
        statusLabel->setParent(this);
        statusLabel->setGeometry(10, height() - 40, width() - 20, 30);
        statusLabel->show();
        
        // Auto-hide after 2 seconds
        QTimer::singleShot(2000, statusLabel, &QLabel::deleteLater);
    }

private:
    CameraConfig m_camera;
    QLabel* m_mainRtspLabel;
    QLabel* m_externalRtspLabel;
    QListWidget* m_alternativeUrlsList;
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

void MainWindow::showCameraInfo()
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
    
    CameraInfoDialog dialog(camera, this);
    dialog.exec();
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
    QHBoxLayout* topMainLayout = new QHBoxLayout(topWidget);
    
    // Left side - Camera and Service controls
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->addWidget(m_cameraGroupBox);
    leftLayout->addWidget(m_serviceGroupBox);
    
    // Right side - VPN controls
    m_vpnWidget = new VpnWidget;
    m_vpnWidget->setMaximumWidth(300);
    m_vpnWidget->setMinimumWidth(250);
    
    topMainLayout->addWidget(leftWidget, 2);
    topMainLayout->addWidget(m_vpnWidget, 1);
    
    m_mainSplitter->addWidget(topWidget);
    m_mainSplitter->addWidget(m_logGroupBox);
    m_mainSplitter->setSizes({500, 200});
    
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
            this, &MainWindow::showCameraInfo);// Camera buttons
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
            this, &MainWindow::onConfigurationChanged);    // Logger
    connect(&Logger::instance(), &Logger::logMessage,
            this, &MainWindow::onLogMessage);
    
    // VPN Widget
    connect(m_vpnWidget, &VpnWidget::statusChanged,
            [this](const QString& status) {
                showMessage(QString("VPN Status: %1").arg(status));
            });
    connect(m_vpnWidget, &VpnWidget::logMessage,
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
