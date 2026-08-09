#pragma once

#include "AppConfig.h"

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

/** 虚拟屏后端：仅使用 Parsec VDD（ioctl 加/删屏 + 保活 ping）。 */
class VddService : public QObject
{
    Q_OBJECT
public:
    explicit VddService(QObject *parent = nullptr);
    ~VddService() override;

    bool driverReady() const;
    bool parsecReady() const;
    QString applyConfig(const AppConfig &cfg, QString *detail = nullptr);
    QString clearVirtualDisplays();
    QString installDriverHint() const;

signals:
    void progress(const QString &msg);

private:
    bool ensureParsecOpen(QString *err);
    void startPing();
    void stopPing();
    void closeParsec();
    bool writeParsecCustomModes(const AppConfig &cfg, QString *err);
    int countDesktopVirtuals() const;

    void *m_parsec = nullptr; // HANDLE
    QVector<int> m_parsecIndices;
    QTimer *m_ping = nullptr;
};
