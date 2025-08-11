#include "AuthDialog.h"
#include <QtWidgets>
#include <QtNetwork>
#include <QSettings>

AuthDialog::AuthDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Visco Connect - Authentication");
    setFixedSize(380, 300);
    setModal(true);

    /* ----------- Branding section ----------- */
    m_logoLbl  = new QLabel(this);
    QPixmap logo(":/icons/logo.png");          // add your logo to resources
    if (!logo.isNull())
        m_logoLbl->setPixmap(logo.scaled(48,48,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    else
        m_logoLbl->setText("🔐");              // fallback

    m_titleLbl = new QLabel("Visco Connect", this);
    m_titleLbl->setStyleSheet("font-size:18px; font-weight:600;");

    m_subLbl   = new QLabel("Secure Authentication Portal", this);
    m_subLbl->setStyleSheet("color:gray;");

    QVBoxLayout *brandLay = new QVBoxLayout;
    brandLay->addWidget(m_logoLbl, 0, Qt::AlignHCenter);
    brandLay->addWidget(m_titleLbl,0, Qt::AlignHCenter);
    brandLay->addWidget(m_subLbl, 0, Qt::AlignHCenter);

    /* ----------- Credentials section ----------- */
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText("Username");

    m_passEdit = new QLineEdit(this);
    m_passEdit->setPlaceholderText("Password");
    m_passEdit->setEchoMode(QLineEdit::Password);

    m_statusLbl = new QLabel(this);
    m_statusLbl->setAlignment(Qt::AlignCenter);
    m_statusLbl->setWordWrap(true);

    m_loginBtn = new QPushButton("Sign In", this);
    m_loginBtn->setEnabled(false);

    /* ----------- Layout root ----------- */
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addLayout(brandLay);
    root->addSpacing(10);
    root->addWidget(m_userEdit);
    root->addWidget(m_passEdit);
    root->addWidget(m_statusLbl);
    root->addWidget(m_loginBtn);

    /* ----------- Connections ----------- */
    connect(m_userEdit, &QLineEdit::textChanged, this, &AuthDialog::updateButtonState);
    connect(m_passEdit, &QLineEdit::textChanged, this, &AuthDialog::updateButtonState);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &AuthDialog::onLoginClicked);
    connect(m_loginBtn,&QPushButton::clicked, this,&AuthDialog::onLoginClicked);

    m_netMgr = new QNetworkAccessManager(this);
    showStatus("Enter your credentials.", Qt::darkGray);
}

AuthDialog::~AuthDialog()
{
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); }
}

/* ---------- helpers ---------- */
void AuthDialog::updateButtonState()
{
    bool ok = !m_userEdit->text().trimmed().isEmpty() && !m_passEdit->text().isEmpty();
    m_loginBtn->setEnabled(ok);
}

void AuthDialog::showStatus(const QString &text,const QColor &col)
{
    m_statusLbl->setText(text);
    m_statusLbl->setStyleSheet(QString("color:%1;").arg(col.name()));
}

/* ---------- login ---------- */
void AuthDialog::onLoginClicked()
{
    if (!m_loginBtn->isEnabled()) return;
    performAuthentication(m_userEdit->text().trimmed(), m_passEdit->text());
}

void AuthDialog::performAuthentication(const QString &user,const QString &pass)
{
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); }

    showStatus("Authenticating…", Qt::darkGray);
    m_loginBtn->setEnabled(false);

    QNetworkRequest req(QUrl("http://3.82.200.187:8086/auth/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");

    QJsonObject obj{{"username",user},{"password",pass}};
    m_reply = m_netMgr->post(req, QJsonDocument(obj).toJson());

    connect(m_reply, &QNetworkReply::finished, this,&AuthDialog::onNetworkFinished);
    connect(m_reply, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred),
            this,&AuthDialog::onNetworkError);
}

void AuthDialog::onNetworkFinished()
{
    if (!m_reply) return;
    int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_reply->readAll();
    m_reply->deleteLater();
    m_reply=nullptr;    if (code==200) {
        QJsonDocument doc=QJsonDocument::fromJson(data);
        QString token=doc.object().value("access_token").toString();
        if (!token.isEmpty()) {
            // Save the token to QSettings
            QSettings s("ViscoConnect","Auth");
            s.setValue("access_token", token);
            // Set expiration time (assume 1 hour if not provided by server)
            qint64 expiresAt = QDateTime::currentSecsSinceEpoch() + 3600; // 1 hour
            s.setValue("expires_at", expiresAt);
            
            showStatus("Login successful.", Qt::darkGreen);
            QTimer::singleShot(700, this, &QDialog::accept);
            return;
        }
        showStatus("Unexpected response.", Qt::red);
    } else if (code==401) {
        showStatus("Invalid username or password.", Qt::red);
    } else {
        showStatus(QString("Server error (%1).").arg(code), Qt::red);
    }
    m_loginBtn->setEnabled(true);
    m_passEdit->clear(); m_passEdit->setFocus();
}

void AuthDialog::onNetworkError(QNetworkReply::NetworkError)
{
    if (!m_reply) return;
    QString err=m_reply->errorString();
    m_reply->deleteLater(); m_reply=nullptr;
    showStatus("Network error: "+err, Qt::red);
    m_loginBtn->setEnabled(true);
}

/* ---------- token helpers unchanged ---------- */
QString AuthDialog::getCurrentAuthToken()
{
    QSettings s("ViscoConnect","Auth");
    QString tok=s.value("access_token").toString();
    qint64 exp=s.value("expires_at").toLongLong();
    return (!tok.isEmpty() && QDateTime::currentSecsSinceEpoch()<exp)?tok:QString();
}
void AuthDialog::clearCurrentAuthToken(){ QSettings("ViscoConnect","Auth").clear(); }
