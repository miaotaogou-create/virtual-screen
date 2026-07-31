#pragma once

#include "AppConfig.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QVBoxLayout;

/** 设置对话框（模态）。 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadFrom(const AppConfig &cfg);
    AppConfig toConfig(const AppConfig &base) const;

signals:
    void applyRequested();
    void saveRequested();
    void clearRequested();

private:
    struct Row {
        QLineEdit *label = nullptr;
        QLineEdit *width = nullptr;
        QLineEdit *height = nullptr;
        QLineEdit *scale = nullptr;
        QLineEdit *hz = nullptr;
    };
    QVBoxLayout *m_rows = nullptr;
    QVector<Row> m_rowEdits;
    void rebuildRows(int count);
};
