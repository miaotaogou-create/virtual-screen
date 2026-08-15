#pragma once

#include "AppConfig.h"
#include "TopologyCanvas.h"
#include "WinDisplay.h"
#include <QWidget>
#include <functional>

class SchemeComboBox;
class TitleBar;
class PropertiesDrawer;
class SettingsDialog;
class VddService;
class QPushButton;
class QTimer;
class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void onApply();
    void onClear();
    void onInstallDriver();

protected:
    void changeEvent(QEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void onAddDisplay();
    void onRefreshDisplays();
    void onPlaceWindow(int index);
    void refreshPreview();
    void selectDisplay(int index);
    void onTopologyArranged(const QVector<QPoint> &origins);
    void onSaveProfileAs();
    void onLoadProfile(const QString &path);
    void onDeleteProfile(const QString &path);
    void showPresetHub();
    void onSchemeComboChanged(int index);

private:
    void setupTopToolbar(QVBoxLayout *root);
    void setupMonitorChrome(QVBoxLayout *root);
    void rebuildBottomTabs();
    void updateScreenInfoBar();
    void updateMonitorChromeVisibility();
    void syncDisplayUi();
    void rebuildSchemeCombo();
    void updateDriverUi();
    void setBusyUi(bool busy);
    void openDriverPage();
    QString bundledDriverInstaller() const;
    bool confirmElevate(const QString &action);
    void runBg(const std::function<QString()> &work, const QString &title);
    void persistCfg();
    DisplaySpec defaultSpec(int ordinal) const;
    void addDisplaySpec(const DisplaySpec &spec);
    void removeDisplayAt(int index);
    void updateDisplayAt(int index, const DisplaySpec &spec);
    QVector<MonitorInfo> matchedVirtuals() const;
    int hitTestBorder(const QPoint &pos) const;
    void capturePreviewForIndex(int index, const QSize &targetSize);
    void syncDrawerCast();
    void showFocusFullscreen();
    void saveFocusScreenshot();

    struct DisplayCastInfo {
        QString title;
        QString processName;
    };

    AppConfig m_cfg;
    TitleBar *m_title = nullptr;
    TopologyCanvas *m_canvas = nullptr;
    PropertiesDrawer *m_drawer = nullptr;
    QWidget *m_toolbar = nullptr;
    SchemeComboBox *m_schemeCombo = nullptr;
    QPushButton *m_schemeHubBtn = nullptr;
    QPushButton *m_placeBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_drawerToggle = nullptr;
    QWidget *m_screenInfoBar = nullptr;
    QLabel *m_screenInfoLabel = nullptr;
    QWidget *m_bottomTabBar = nullptr;
    QHBoxLayout *m_bottomTabLayout = nullptr;
    QPushButton *m_bottomPlaceBtn = nullptr;
    QPushButton *m_bottomRemoveBtn = nullptr;
    SettingsDialog *m_settings = nullptr;
    VddService *m_vdd = nullptr;
    QTimer *m_timer = nullptr;
    int m_tabIndex = 0;
    bool m_busy = false;
    bool m_grabBusy = false;
    int m_pendingGrabs = 0;
    DisplaySpec m_pendingSpec;
    int m_pendingIndex = -1;
    QString m_activeProfilePath;
    QVector<DisplayCastInfo> m_castByDisplay;
};
