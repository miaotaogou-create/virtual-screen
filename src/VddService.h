#pragma once

#include "AppConfig.h"

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

/** 虚拟屏后端：仅 Parsec VDD；支持整表应用，也支持单块加/删/改。 */
class VddService : public QObject
{
    Q_OBJECT
public:
    explicit VddService(QObject *parent = nullptr);
    ~VddService() override;

    bool driverReady() const;
    bool parsecReady() const;
    QString installDriverHint() const;

    /** 按配置重建全部虚拟屏（加载方案时用）。 */
    QString applyConfig(const AppConfig &cfg, QString *detail = nullptr);
    QString clearVirtualDisplays();

    /** 单块操作（交互式加屏）。成功时同步更新内部索引列表。 */
    QString addOne(const DisplaySpec &spec);
    QString removeAt(int index);
    /** allDisplays：改模式时一并写入注册表，避免冲掉其它屏的自定义模式。 */
    QString updateAt(int index, const DisplaySpec &spec, const QVector<DisplaySpec> &allDisplays);

    int trackedCount() const { return m_parsecIndices.size(); }

signals:
    void progress(const QString &msg);

private:
    bool ensureParsecOpen(QString *err);
    void startPing();
    void stopPing();
    void closeParsec();
    bool writeParsecCustomModes(const QVector<DisplaySpec> &displays, QString *err);
    bool writeParsecCustomModes(const AppConfig &cfg, QString *err);
    int countDesktopVirtuals() const;
    QString arrangeAndDpi(const AppConfig &cfg);

    void *m_parsec = nullptr;
    QVector<int> m_parsecIndices;
    QTimer *m_ping = nullptr;
};
