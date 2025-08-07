#include "AuthDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QPixmap>
#include <QFont>
#include <QTimer>
#include <QApplication>
#include <QPropertyAnimation>

AuthDialog::AuthDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainFrame(nullptr)
    , m_headerFrame(nullptr)
    , m_formFrame(nullptr)
    , m_logoLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_subtitleLabel(nullptr)
    , m_welcomeLabel(nullptr)
    , m_usernameLabel(nullptr)
    , m_usernameEdit(nullptr)
    , m_passwordLabel(nullptr)
    , m_passwordEdit(nullptr)
    , m_loginButton(nullptr)
    , m_mainLayout(nullptr)
    , m_headerLayout(nullptr)
    , m_formLayout(nullptr)
{
    setupUI();
    setupStyles();
    setupConnections();
    
    // Initial welcome message
    showMessage("Welcome! Please sign in to continue.", "welcome");
}

AuthDialog::~AuthDialog()
{
    // Qt handles cleanup automatically for child widgets
}

void AuthDialog::setupUI()
{
    // Window configuration
    setWindowIcon(QIcon(":/icons/logo.ico"));
    setFixedSize(460, 580);
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    
    // Main container frame (simplified)
    m_mainFrame = new QFrame(this);
    m_mainFrame->setObjectName("mainFrame");
    
    // Header section (simplified)
    m_headerFrame = new QFrame(m_mainFrame);
    m_headerFrame->setObjectName("headerFrame");
    
    // Logo
    m_logoLabel = new QLabel(m_headerFrame);
    m_logoLabel->setObjectName("logoLabel");
    QPixmap logo(":/icons/logo.png");
    if (!logo.isNull()) {
        m_logoLabel->setPixmap(logo.scaled(75, 75, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_logoLabel->setText("🔐");
        m_logoLabel->setStyleSheet("font-size: 44px;");
    }
    m_logoLabel->setAlignment(Qt::AlignCenter);
    
    // Title
    m_titleLabel = new QLabel("Visco Connect", m_headerFrame);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    
    // Subtitle
    m_subtitleLabel = new QLabel("Secure Authentication Portal", m_headerFrame);
    m_subtitleLabel->setObjectName("subtitleLabel");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    
    // Welcome message (no box, integrated directly with wider width)
    m_welcomeLabel = new QLabel(m_mainFrame);
    m_welcomeLabel->setObjectName("welcomeLabel");
    m_welcomeLabel->setAlignment(Qt::AlignLeft);
    m_welcomeLabel->setWordWrap(true);
    m_welcomeLabel->setContentsMargins(2, 8, 2, 8);
    m_welcomeLabel->setMaximumWidth(600);
    m_welcomeLabel->setMinimumHeight(50);
    
    // Form section (single clean container)
    m_formFrame = new QFrame(m_mainFrame);
    m_formFrame->setObjectName("formFrame");
    
    // Username field
    m_usernameLabel = new QLabel("Username", m_formFrame);
    m_usernameLabel->setObjectName("fieldLabel");
    
    m_usernameEdit = new QLineEdit(m_formFrame);
    m_usernameEdit->setObjectName("inputField");
    m_usernameEdit->setPlaceholderText("Enter your username");
    m_usernameEdit->setMinimumHeight(48);
    
    // Password field
    m_passwordLabel = new QLabel("Password", m_formFrame);
    m_passwordLabel->setObjectName("fieldLabel");
    
    m_passwordEdit = new QLineEdit(m_formFrame);
    m_passwordEdit->setObjectName("inputField");
    m_passwordEdit->setPlaceholderText("Enter your password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumHeight(48);
    
    // Login button
    m_loginButton = new QPushButton("Sign In", m_formFrame);
    m_loginButton->setObjectName("loginButton");
    m_loginButton->setMinimumHeight(52);
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setEnabled(false);
    
    // Setup layouts
    setupLayouts();
    
    // Add subtle shadow effect only to main frame
    setupShadowEffects();
}

void AuthDialog::setupLayouts()
{
    // Header layout (more compact)
    m_headerLayout = new QVBoxLayout(m_headerFrame);
    m_headerLayout->setSpacing(12);
    m_headerLayout->setContentsMargins(25, 25, 25, 20);
    m_headerLayout->addWidget(m_logoLabel);
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addWidget(m_subtitleLabel);
    
    // Form layout (better spacing)
    m_formLayout = new QVBoxLayout(m_formFrame);
    m_formLayout->setSpacing(18);
    m_formLayout->setContentsMargins(35, 30, 35, 30);
    
    m_formLayout->addWidget(m_usernameLabel);
    m_formLayout->addWidget(m_usernameEdit);
    m_formLayout->addSpacing(8);
    m_formLayout->addWidget(m_passwordLabel);
    m_formLayout->addWidget(m_passwordEdit);
    m_formLayout->addSpacing(20);
    m_formLayout->addWidget(m_loginButton);
    
    // Main layout (reduced side margins for wider welcome text)
    m_mainLayout = new QVBoxLayout(m_mainFrame);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_headerFrame);
    m_mainLayout->addWidget(m_welcomeLabel, 0, Qt::AlignCenter);
    m_mainLayout->addSpacing(5);
    m_mainLayout->addWidget(m_formFrame);
    m_mainLayout->addStretch();
    
    // Dialog layout (reduced margins for wider content)
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setContentsMargins(8, 18, 8, 18);
    dialogLayout->addWidget(m_mainFrame);
}

void AuthDialog::setupShadowEffects()
{
    // Only main frame shadow for cleaner look
    QGraphicsDropShadowEffect *mainShadow = new QGraphicsDropShadowEffect(this);
    mainShadow->setBlurRadius(25);
    mainShadow->setColor(QColor(0, 0, 0, 35));
    mainShadow->setOffset(0, 6);
    m_mainFrame->setGraphicsEffect(mainShadow);
}

void AuthDialog::setupStyles()
{
    // Cleaner, more refined styles with wider welcome text
    setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8f9fa, stop:1 #e9ecef);
        }
        
        QFrame#mainFrame {
            background: white;
            border-radius: 16px;
            border: 1px solid rgba(0, 123, 255, 0.1);
        }
        
        QFrame#headerFrame {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #007bff, stop:1 #0056b3);
            border-radius: 16px 16px 0px 0px;
            color: white;
        }
        
        QFrame#formFrame {
            background: transparent;
            border: none;
            margin: 0px 20px 20px 20px;
        }
        
        QLabel#titleLabel {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 26px;
            font-weight: bold;
            color: white;
            margin: 4px 0px;
        }
        
        QLabel#subtitleLabel {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 13px;
            color: rgba(255, 255, 255, 0.9);
            font-weight: 400;
            margin-bottom: 5px;
        }
        
        QLabel#welcomeLabel {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 15px;
            color: #6c757d;
            margin: 8px 5px;
            font-weight: 400;
            max-width: 430px;       /* Increased to 430px */
            min-height: 40px;  
        }
        
        QLabel#fieldLabel {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 14px;
            font-weight: 600;
            color: #495057;
            margin-bottom: 6px;
        }
        
        QLineEdit#inputField {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 15px;
            padding: 14px 18px;
            border: 2px solid #e1e8ed;
            border-radius: 10px;
            background-color: #ffffff;
            selection-background-color: #007bff;
        }
        
        QLineEdit#inputField:focus {
            border-color: #007bff;
            outline: none;
            background-color: #ffffff;
        }
        
        QLineEdit#inputField:hover {
            border-color: #b8d4f0;
        }
        
        QPushButton#loginButton {
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 16px;
            font-weight: 600;
            color: white;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #007bff, stop:1 #0056b3);
            border: none;
            border-radius: 12px;
            padding: 16px;
        }
        
        QPushButton#loginButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0069d9, stop:1 #004085);
        }
        
        QPushButton#loginButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #004085, stop:1 #002752);
        }
        
        QPushButton#loginButton:disabled {
            background: #adb5bd;
            color: #ffffff;
        }
    )");
}

void AuthDialog::setupConnections()
{
    connect(m_loginButton, &QPushButton::clicked, this, &AuthDialog::onLoginClicked);
    connect(m_usernameEdit, &QLineEdit::textChanged, this, &AuthDialog::onUsernameChanged);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &AuthDialog::onPasswordChanged);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &AuthDialog::onLoginClicked);
}

void AuthDialog::onUsernameChanged()
{
    updateLoginButtonState();
}

void AuthDialog::onPasswordChanged()
{
    updateLoginButtonState();
}

void AuthDialog::updateLoginButtonState()
{
    bool enableLogin = !m_usernameEdit->text().trimmed().isEmpty() && 
                       !m_passwordEdit->text().isEmpty();
    m_loginButton->setEnabled(enableLogin);
}

void AuthDialog::showMessage(const QString &message, const QString &type)
{
    m_welcomeLabel->setText(message);
    
    // Simple text styling without boxes - with wider display
    if (type == "error") {
        m_welcomeLabel->setStyleSheet(R"(
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 15px;
            color: #dc3545;
            margin: 8px 5px;
            font-weight: 500;
            max-width: 700px;
            min-height: 40px;
        )");
    } else if (type == "success") {
        m_welcomeLabel->setStyleSheet(R"(
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 15px;
            color: #28a745;
            margin: 8px 10px;
            font-weight: 500;
            max-width: 700px;
            min-height: 40px;

        )");
    } else {
        m_welcomeLabel->setStyleSheet(R"(
            font-family: 'Segoe UI', 'Arial', sans-serif;
            font-size: 15px;
            color: #6c757d;
            margin: 8px 10px;
            font-weight: 400;
            max-width: 700px;
            min-height: 40px;
        )");
    }
}

void AuthDialog::onLoginClicked()
{
    if (!m_loginButton->isEnabled()) {
        return;
    }
    
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();
    
    // Disable the button and show loading state
    m_loginButton->setEnabled(false);
    m_loginButton->setText("Signing In...");
    showMessage("Authenticating credentials...", "info");
    
    // Simulate authentication delay
    QTimer::singleShot(1000, this, [this, username, password]() {
        if (username == "shiven" && password == "shiven") {
            showMessage("✓ Authentication successful! Welcome back.", "success");
            m_loginButton->setText("Success!");
            
            // Auto-close after success message
            QTimer::singleShot(1200, this, &QDialog::accept);
        } else {
            showMessage("✗ Invalid credentials. Please check your username and password.", "error");
            m_loginButton->setText("Sign In");
            m_loginButton->setEnabled(true);
            
            // Clear password field on failed login
            m_passwordEdit->clear();
            m_passwordEdit->setFocus();
        }
    });
}
