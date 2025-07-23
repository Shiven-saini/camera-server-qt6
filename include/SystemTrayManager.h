#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>

class MainWindow;
class CameraManager;

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(MainWindow* mainWindow, CameraManager* cameraManager, QObject *parent = nullptr);
    ~SystemTrayManager();
      void initialize();
    void show();
    void hide();
      bool isVisible() const;
    void updateCameraStatus();
    void showNotification(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    void notifyCameraStatusChange(const QString& cameraName, bool started);

signals:
    void showMainWindow();
    void enableAllCameras();
    void disableAllCameras();
    void quitApplication();

private slots:
    void handleTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleShowMainWindow();    void handleEnableAllCameras();
    void handleDisableAllCameras();
    void handleQuitApplication();

private:
    void createTrayIcon();
    void createContextMenu();
    void updateTrayIconToolTip();
    
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
    
    // Actions
    QAction* m_showAction;
    QAction* m_enableAllAction;
    QAction* m_disableAllAction;
    QAction* m_separatorAction;
    QAction* m_quitAction;
    
    MainWindow* m_mainWindow;
    CameraManager* m_cameraManager;
};

#endif // SYSTEMTRAYMANAGER_H
