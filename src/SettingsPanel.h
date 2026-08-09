#pragma once

#include "AppConfig.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QSpinBox;
class QLabel;
class QVBoxLayout;
class QComboBox;
class QPushButton;

/** 设置对话框：可增减虚拟屏，下拉切换/另存项目配置。 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadFrom(const AppConfig &cfg);
    AppConfig toConfig(const AppConfig &base) const;
    void setProfileHint(const QString &name);
    void refreshProfileList(const QString &selectName = QString());
    void setDriverHint(const QString &text);

signals:
    void saveRequested();
    void saveAsRequested();
    void loadProfileRequested(const QString &path);
    void browseLoadRequested();

private slots:
    void addDisplay();
    void removeLastDisplay();
    void onProfileComboChanged(int index);

private:
    struct Row {
        QLineEdit *label = nullptr;
        QSpinBox *width = nullptr;
        QSpinBox *height = nullptr;
        QSpinBox *scale = nullptr;
        QSpinBox *hz = nullptr;
    };
    QLabel *m_profileHint = nullptr;
    QLabel *m_driverHint = nullptr;
    QComboBox *m_profileCombo = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QVector<Row> m_rowEdits;
    void rebuildRows(int count);
    void fillRow(int index, const DisplaySpec &s);
};
