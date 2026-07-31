#pragma once

#include "AppConfig.h"

#include <QVector>
#include <QWidget>

class QLineEdit;
class QVBoxLayout;

class SettingsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void loadFrom(const AppConfig &cfg);
    AppConfig toConfig(const AppConfig &base) const;

signals:
    void applyRequested();
    void saveRequested();
    void clearRequested();
    void closeRequested();

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
