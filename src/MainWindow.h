#pragma once

#include "AppConfig.h"
#include "WinDisplay.h"

#include <QWidget>
#include <functional>

class TitleBar;
class PreviewPane;
class SettingsDialog;
class VddService;
class QPushButton;
class QLabel;
class QTimer;
class QHBoxLayout;
class QMenu;

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
    void resizeEvent(QResizeEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void onAddDisplay();
    void onCustomDialog();
    void onRefreshDisplays();
    void onPlaceWindow();
    void togglePreview();
    void refreshPreview();
    void selectTab(int index);
    void onGuidePrimary();
    void onGuideSecondary();
    void onSaveProfile();
    void onSaveProfileAs();
    void onLoadProfile(const QString &path);
    void onDeleteProfile(const QString &path);
    void showProfileMenu();
    void showDisplayContextMenu(int index, const QPoint &globalPos);

private:
    void rebuildTabs();
    void refreshGuide();
    void updateDriverUi();
    void setPreviewEnabled(bool on);
    void setBusyUi(bool busy);
    void openDriverPage();
    QString bundledDriverInstaller() const;
    bool confirmElevate(const QString &action);
    bool ensureAdminFor(const QString &action, const QStringList &args);
    void runBg(const std::function<QString()> &work, const QString &title);
    void persistCfg();
    DisplaySpec defaultSpec(int ordinal) const;
    void addDisplaySpec(const DisplaySpec &spec);
    void removeDisplayAt(int index);
    void updateDisplayAt(int index, const DisplaySpec &spec);
    QVector<MonitorInfo> matchedVirtuals() const;
    int hitTestBorder(const QPoint &pos) const;

    AppConfig m_cfg;
    TitleBar *m_title = nullptr;
    QWidget *m_tabBar = nullptr;
    QHBoxLayout *m_tabLay = nullptr;
    QPushButton *m_previewToggle = nullptr;
    PreviewPane *m_preview = nullptr;
    QWidget *m_bottom = nullptr;
    SettingsDialog *m_settings = nullptr;
    VddService *m_vdd = nullptr;
    QTimer *m_timer = nullptr;
    QVector<QPushButton *> m_tabs;
    int m_tabIndex = 0;
    bool m_previewOn = false;
    bool m_busy = false;
    bool m_grabBusy = false;
    /** runBg 成功后写回配置用。 */
    DisplaySpec m_pendingSpec;
    int m_pendingIndex = -1;
};
