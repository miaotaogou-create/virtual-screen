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
class QTimer;
class QHBoxLayout;

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
    void refreshPreview();
    void selectTab(int index);
    void onSaveProfile();
    void onSaveProfileAs();
    void onLoadProfile(const QString &path);
    void onDeleteProfile(const QString &path);
    void showProfileMenu();
    void showDisplayContextMenu(int index, const QPoint &globalPos);
    void onPreviewPointer(qreal nx, qreal ny, Qt::MouseButton button, bool pressed, int wheelDelta);
    void onPreviewKey(int key, Qt::KeyboardModifiers mods, bool pressed);

private:
    void rebuildTabs();
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

    AppConfig m_cfg;
    TitleBar *m_title = nullptr;
    QHBoxLayout *m_tabLay = nullptr;
    PreviewPane *m_preview = nullptr;
    QWidget *m_bottom = nullptr;
    SettingsDialog *m_settings = nullptr;
    VddService *m_vdd = nullptr;
    QTimer *m_timer = nullptr;
    QVector<QPushButton *> m_tabs;
    int m_tabIndex = 0;
    bool m_busy = false;
    bool m_grabBusy = false;
    DisplaySpec m_pendingSpec;
    int m_pendingIndex = -1;
    QRect m_previewGeo;
    QPoint m_savedCursor;
    bool m_cursorSaved = false;
};
