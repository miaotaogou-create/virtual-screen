#pragma once

#include "AppConfig.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QLabel;
class QVBoxLayout;

/** 设置对话框：可增减虚拟屏，可加载/另存项目配置。 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadFrom(const AppConfig &cfg);
    AppConfig toConfig(const AppConfig &base) const;
    void setProfileHint(const QString &name);

signals:
    void applyRequested();
    void saveRequested();
    void saveAsRequested();
    void loadRequested();
    void clearRequested();

private slots:
    void addDisplay();
    void removeLastDisplay();

private:
    struct Row {
        QLineEdit *label = nullptr;
        QLineEdit *width = nullptr;
        QLineEdit *height = nullptr;
        QLineEdit *scale = nullptr;
        QLineEdit *hz = nullptr;
    };
    QLabel *m_profileHint = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QVector<Row> m_rowEdits;
    void rebuildRows(int count);
    void fillRow(int index, const DisplaySpec &s);
};
