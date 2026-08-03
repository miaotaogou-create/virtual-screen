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

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void onApply();
    void onClear();

protected:
    void changeEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void toggleSettings();
    void onSaveSettings();
    void onSaveAsSettings();
    void onLoadProfile(const QString &path);
    void onBrowseLoadSettings();
    void togglePreview();
    void refreshPreview();
    void selectTab(int index);

private:
    void rebuildTabs();
    void updateDriverUi();
    void runBg(const std::function<QString()> &work, const QString &title);
    /** 按配置顺序匹配虚拟屏：先分辨率，再从左到右。 */
    QVector<MonitorInfo> matchedVirtuals() const;
    int hitTestBorder(const QPoint &pos) const;

    AppConfig m_cfg;
    TitleBar *m_title = nullptr;
    QWidget *m_tabBar = nullptr;
    QHBoxLayout *m_tabLay = nullptr;
    QPushButton *m_previewToggle = nullptr;
    PreviewPane *m_preview = nullptr;
    SettingsDialog *m_settings = nullptr;
    VddService *m_vdd = nullptr;
    QTimer *m_timer = nullptr;
    QVector<QPushButton *> m_tabs;
    int m_tabIndex = 0;
    bool m_previewOn = false; // 默认关：抓屏贵，需要时再开
    bool m_busy = false;
    bool m_grabBusy = false;
};
