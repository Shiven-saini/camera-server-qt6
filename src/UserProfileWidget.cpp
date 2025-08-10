#include "UserProfileWidget.h"

#include <QApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include "AuthDialog.h"
#include "MainWindow.h"
#include "Logger.h"

UserProfileWidget::UserProfileWidget(QWidget *parent)
    : QWidget(parent)
    , m_profileGroup(nullptr)
    , m_fullNameLabel(nullptr)
    , m_emailLabel(nullptr)
    , m_logoutButton(nullptr)
    , m_avatarLabel(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_profileReply(nullptr)
{
    setupUI();
    connectSignals();
    
    // Fetch user profile on initialization
    fetchUserProfile();
}

UserProfileWidget::~UserProfileWidget()
{
    if (m_profileReply) {
        m_profileReply->abort();
        m_profileReply->deleteLater();
    }
}

void UserProfileWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Create profile group with consistent styling
    m_profileGroup = new QGroupBox(tr("User Account"));
    m_profileGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #cccccc;"
        "    border-radius: 6px;"
        "    margin-top: 1ex;"
        "    padding-top: 8px;"
        "    background-color: transparent;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );

    // Create a clean embedded layout without separate background
    QWidget *profileCard = new QWidget();
    profileCard->setStyleSheet(
        "QWidget {"
        "    background-color: transparent;"
        "    border: none;"
        "    padding: 8px;"
        "}"
    );

    QHBoxLayout *cardLayout = new QHBoxLayout(profileCard);
    cardLayout->setSpacing(12);    // Avatar with more subtle styling
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(40, 40);
    m_avatarLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #6c757d;"
        "    border-radius: 20px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 16px;"
        "    border: 2px solid #e9ecef;"
        "}"
    );
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setText("?");    // User info section with compact spacing
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(0, 0, 0, 0);m_fullNameLabel = new QLabel(tr("Loading..."));
    m_fullNameLabel->setStyleSheet(
        "QLabel {"
        "    font-weight: bold;"
        "    font-size: 13px;"
        "    color: #333333;"
        "    margin-bottom: 2px;"
        "}"
    );

    m_emailLabel = new QLabel(tr("Loading..."));
    m_emailLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 11px;"
        "    color: #666666;"
        "}"
    );

    infoLayout->addWidget(m_fullNameLabel);
    infoLayout->addWidget(m_emailLabel);
    infoLayout->addStretch();    // Logout button with proper sizing
    m_logoutButton = new QPushButton(tr("Logout"));
    m_logoutButton->setMinimumSize(90, 34);
    m_logoutButton->setMaximumHeight(34);
    m_logoutButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #dc3545;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "    padding: 6px 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #c82333;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #bd2130;"
        "}"    );

    // Button layout section
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->addWidget(m_logoutButton);
    buttonLayout->addStretch();

    // Assemble the card layout with better spacing
    cardLayout->addWidget(m_avatarLabel);
    cardLayout->addLayout(infoLayout, 1);
    cardLayout->addLayout(buttonLayout);

    // Add card to group with minimal padding
    QVBoxLayout *groupLayout = new QVBoxLayout(m_profileGroup);
    groupLayout->setContentsMargins(8, 16, 8, 8);
    groupLayout->addWidget(profileCard);

    mainLayout->addWidget(m_profileGroup);
}

void UserProfileWidget::connectSignals()
{
    connect(m_logoutButton, &QPushButton::clicked, this, &UserProfileWidget::onLogoutClicked);
}

void UserProfileWidget::fetchUserProfile()
{
    if (m_profileReply) {
        m_profileReply->abort();
        m_profileReply->deleteLater();
    }

    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        updateProfileDisplay(tr("Not authenticated"), tr("Please login"));
        return;
    }

    showLoadingState();

    QNetworkRequest request(QUrl("http://3.82.200.187:8086/users/profile"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    m_profileReply = m_networkManager->get(request);

    connect(m_profileReply, &QNetworkReply::finished, this, &UserProfileWidget::onProfileFetchFinished);
    connect(m_profileReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &UserProfileWidget::onProfileFetchError);

    LOG_INFO("Fetching user profile from server", "UserProfileWidget");
}

void UserProfileWidget::onProfileFetchFinished()
{
    if (!m_profileReply) return;

    int statusCode = m_profileReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_profileReply->readAll();
    m_profileReply->deleteLater();
    m_profileReply = nullptr;

    if (statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        QString firstName = obj.value("first_name").toString();
        QString lastName = obj.value("last_name").toString();
        QString email = obj.value("email").toString();
        QString username = obj.value("username").toString();

        // Combine first and last name, fallback to username if names are empty
        QString fullName;
        if (!firstName.isEmpty() || !lastName.isEmpty()) {
            fullName = QString("%1 %2").arg(firstName, lastName).trimmed();
        } else if (!username.isEmpty()) {
            fullName = username;
        } else {
            fullName = tr("Unknown User");
        }

        updateProfileDisplay(fullName, email);
        
        // Update avatar with first letter of name
        if (!fullName.isEmpty()) {
            m_avatarLabel->setText(fullName.at(0).toUpper());
        }

        LOG_INFO(QString("User profile loaded: %1 (%2)").arg(fullName, email), "UserProfileWidget");

    } else if (statusCode == 401) {
        updateProfileDisplay(tr("Authentication Failed"), tr("Please login again"));
        LOG_WARNING("User profile fetch failed: Authentication token invalid", "UserProfileWidget");
    } else {
        updateProfileDisplay(tr("Profile Error"), tr("Failed to load profile"));
        LOG_WARNING(QString("User profile fetch failed with status code: %1").arg(statusCode), "UserProfileWidget");
    }
}

void UserProfileWidget::onProfileFetchError(QNetworkReply::NetworkError error)
{
    if (!m_profileReply) return;

    QString errorString = m_profileReply->errorString();
    m_profileReply->deleteLater();
    m_profileReply = nullptr;

    updateProfileDisplay(tr("Network Error"), tr("Unable to load profile"));
    LOG_ERROR(QString("User profile fetch network error: %1").arg(errorString), "UserProfileWidget");
}

void UserProfileWidget::updateProfileDisplay(const QString &fullName, const QString &email)
{
    m_currentFullName = fullName;
    m_currentEmail = email;

    m_fullNameLabel->setText(fullName);
    m_emailLabel->setText(email);
}

void UserProfileWidget::showLoadingState()
{
    m_fullNameLabel->setText(tr("Loading..."));
    m_emailLabel->setText(tr("Fetching profile..."));
    m_avatarLabel->setText("...");
}

void UserProfileWidget::onLogoutClicked()
{
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Logout Confirmation"),
        tr("Are you sure you want to logout? The application will close and you will need to authenticate again when you restart it."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        LOG_INFO("User confirmed logout, preparing to close application", "UserProfileWidget");

        // Find VPN widget and disconnect if needed
        QWidget *parentWidget = this->parentWidget();
        while (parentWidget) {
            if (MainWindow *mainWindow = qobject_cast<MainWindow*>(parentWidget)) {
                // Access the VPN widget through the main window if needed
                // For now, we'll just let the main window handle VPN disconnection
                break;
            }
            parentWidget = parentWidget->parentWidget();
        }

        // Clear the authentication token
        AuthDialog::clearCurrentAuthToken();

        // Delete the WireGuard config file
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(appDataPath);
        QString configPath = dir.filePath("wireguard_server.conf");
        
        if (QFile::exists(configPath)) {
            if (QFile::remove(configPath)) {
                LOG_INFO("Deleted WireGuard config file: " + configPath, "UserProfileWidget");
            } else {
                LOG_WARNING("Failed to delete WireGuard config file: " + configPath, "UserProfileWidget");
            }
        }

        // Clear saved WireGuard config from QSettings
        QSettings settings("ViscoConnect", "WireGuard");
        settings.clear();
        LOG_INFO("Cleared WireGuard settings", "UserProfileWidget");

        LOG_INFO("User logged out successfully, closing application", "UserProfileWidget");

        // Find the main window and set force quit flag before closing
        MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
        if (mainWindow) {
            mainWindow->setForceQuit(true);
            mainWindow->close();
        } else {
            // Fallback: Force application exit
            QApplication::quit();
        }
    }
}
