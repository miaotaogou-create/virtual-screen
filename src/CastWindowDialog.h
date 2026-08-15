#pragma once

#include "WinDisplay.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QListWidget;

class CastWindowDialog : public QDialog
{
    Q_OBJECT
public:
    CastWindowDialog(const QString &targetLabel, const MonitorInfo &targetMonitor,
                     QWidget *parent = nullptr);

    qulonglong selectedHwnd() const { return m_selectedHwnd; }
    QString selectedTitle() const { return m_selectedTitle; }
    QString selectedProcessName() const { return m_selectedProcessName; }

private:
    void refreshList();
    void castToTarget();

    MonitorInfo m_target;
    QString m_targetLabel;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QVector<TopWindowInfo> m_windows;
    qulonglong m_selectedHwnd = 0;
    QString m_selectedTitle;
    QString m_selectedProcessName;
};
