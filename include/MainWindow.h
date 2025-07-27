#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QStatusBar>
#include <QScrollBar>
#include <QCloseEvent>
#include <QProcess>
#include <QTimer>
#include "CameraManager.h"
#include "SystemTrayManager.h"
#include "VpnWidget.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    void showMessage(const QString& message);
    void appendLog(const QString& message);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

public slots:
    void editCamera();
    void testCamera();

private slots:
    void addCamera();
    void discoverCameras();
    void showCameraInfo();
    void removeCamera();
    void toggleCamera();
    void startAllCameras();
    void stopAllCameras();
    void toggleAutoStart();
    void showAbout();
    void onCameraSelectionChanged();
    void onCameraStarted(const QString& id);
    void onCameraStopped(const QString& id);
    void onCameraError(const QString& id, const QString& error);
    void onConfigurationChanged();
    void onLogMessage(const QString& message);
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void createMenuBar();
    void createStatusBar();
    void createCentralWidget();
    void setupConnections();
    void updateCameraTable();
    void updateButtons();
    void loadSettings();
    void saveSettings();
    
    // UI Components
    QSplitter* m_mainSplitter;
    QWidget* m_centralWidget;
      // Camera management
    QGroupBox* m_cameraGroupBox;
    QTableWidget* m_cameraTable;
    QPushButton* m_addButton;
    QPushButton* m_discoverButton;
    QPushButton* m_editButton;
    QPushButton* m_removeButton;
    QPushButton* m_toggleButton;
    QPushButton* m_testButton;
    
    // Service control
    QGroupBox* m_serviceGroupBox;
    QPushButton* m_startAllButton;
    QPushButton* m_stopAllButton;
    QCheckBox* m_autoStartCheckBox;
    QLabel* m_serviceStatusLabel;
      // Log viewer
    QGroupBox* m_logGroupBox;
    QTextEdit* m_logTextEdit;
    QPushButton* m_clearLogButton;
    
    // VPN Widget
    VpnWidget* m_vpnWidget;
    
    // Menu and actions
    QMenu* m_fileMenu;
    QMenu* m_serviceMenu;
    QMenu* m_helpMenu;
    QAction* m_exitAction;
    QAction* m_installServiceAction;
    QAction* m_uninstallServiceAction;
    QAction* m_aboutAction;
    
    // Core components
    CameraManager* m_cameraManager;
    SystemTrayManager* m_trayManager;
      // State
    bool m_isClosingToTray;
    bool m_forceQuit;
    QProcess* m_pingProcess;
    QString m_currentTestingCameraId;
};

#endif // MAINWINDOW_H
