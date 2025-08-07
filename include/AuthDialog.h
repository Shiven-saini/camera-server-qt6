#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QFrame;
class QVBoxLayout;
class QHBoxLayout;
QT_END_NAMESPACE

class AuthDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AuthDialog(QWidget *parent = nullptr);
    ~AuthDialog();

private slots:
    void onLoginClicked();
    void onUsernameChanged();
    void onPasswordChanged();

private:
    void setupUI();
    void setupStyles();
    void setupConnections();
    void setupLayouts();
    void setupShadowEffects();
    void updateLoginButtonState();
    void showMessage(const QString &message, const QString &type = "info");
    
    // UI Components
    QFrame *m_mainFrame;
    QFrame *m_headerFrame;
    QFrame *m_formFrame;
    
    QLabel *m_logoLabel;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_welcomeLabel;
    
    QLabel *m_usernameLabel;
    QLineEdit *m_usernameEdit;
    QLabel *m_passwordLabel;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginButton;
    
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_headerLayout;
    QVBoxLayout *m_formLayout;
};

#endif // AUTHDIALOG_H
