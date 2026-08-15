#include "PresetHubDialog.h"

#include "AppConfig.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace {

QIcon svgIcon(const QString &resourcePath, const QColor &stroke, int size = 16)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();

    QString xml = QString::fromUtf8(file.readAll());
    xml.replace(QStringLiteral("currentColor"), stroke.name(QColor::HexRgb));

    QSvgRenderer renderer(xml.toUtf8());
    if (!renderer.isValid())
        return QIcon();

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return QIcon(pm);
}

QLabel *makePill(const QString &text, const char *objectName, QWidget *parent)
{
    auto *lab = new QLabel(text, parent);
    lab->setObjectName(QString::fromUtf8(objectName));
    lab->setAlignment(Qt::AlignCenter);
    return lab;
}

QString currentMountSummary(const AppConfig &cfg)
{
    if (cfg.displays.isEmpty())
        return QStringLiteral("当前未挂载虚拟屏");

    QStringList parts;
    for (const DisplaySpec &d : cfg.displays)
        parts << QStringLiteral("%1x%2").arg(d.width).arg(d.height);
    return QStringLiteral("当前已挂载 %1 块虚拟屏 (%2)")
        .arg(cfg.displays.size())
        .arg(parts.join(QStringLiteral(", ")));
}

QString profileCategoryTag(int displayCount)
{
    if (displayCount <= 1)
        return QStringLiteral("轻量推流");
    if (displayCount == 2)
        return QStringLiteral("常用办公");
    return QStringLiteral("多屏工作流");
}

QString profileDescription(const AppConfig &cfg)
{
    if (cfg.displays.isEmpty())
        return QStringLiteral("无虚拟屏配置");

    const int n = cfg.displays.size();
    const int scale = cfg.displays.first().scale;
    if (n == 1)
        return QStringLiteral("极简单虚拟显示器，适合无头服务器或简单投屏推流");
    if (n == 2)
        return QStringLiteral("经典双屏工作台，缩放 %1%").arg(scale);
    return QStringLiteral("%1 块虚拟屏超广工作流，缩放 %2%").arg(n).arg(scale);
}

bool isActiveProfile(const QString &path, const QString &activePath, const AppConfig &current,
                     const QString &baseName)
{
    if (!activePath.isEmpty())
        return QFileInfo(path).absoluteFilePath() == QFileInfo(activePath).absoluteFilePath();
    return current.profileName == baseName;
}

} // namespace

/** 关闭按钮：自绘 ×，避免字体 glyph 在 hover 底里视觉偏移。 */
class PresetCloseButton : public QPushButton
{
public:
    explicit PresetCloseButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setObjectName(QStringLiteral("PresetHubCloseBtn"));
        setFixedSize(36, 36);
        setFlat(true);
        setText(QString());
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 15));
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
        }

        QColor color(0x94, 0xa3, 0xb8);
        if (underMouse())
            color = QColor(0xe2, 0xe8, 0xf0);

        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);

        const qreal inset = width() * 0.37;
        const QRectF box = QRectF(rect());
        p.drawLine(QPointF(box.left() + inset, box.top() + inset),
                   QPointF(box.right() - inset, box.bottom() - inset));
        p.drawLine(QPointF(box.right() - inset, box.top() + inset),
                   QPointF(box.left() + inset, box.bottom() - inset));
    }

    void enterEvent(QEvent *event) override
    {
        QPushButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QPushButton::leaveEvent(event);
        update();
    }
};

PresetHubDialog::PresetHubDialog(const AppConfig &current, const QString &activeProfilePath,
                                 QWidget *parent)
    : QDialog(parent)
    , m_current(current)
    , m_activePath(activeProfilePath)
{
    setObjectName(QStringLiteral("PresetHubDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    resize(700, 680);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(16);

    auto *header = new QHBoxLayout();
    header->setSpacing(12);

    auto *iconLab = new QLabel(this);
    iconLab->setPixmap(svgIcon(QStringLiteral(":/icons/icon_presets_layers.svg"),
                               QColor(0x06, 0xb6, 0xd4), 28)
                       .pixmap(28, 28));
    iconLab->setFixedSize(32, 32);
    header->addWidget(iconLab, 0, Qt::AlignTop);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("方案中心 (Preset Manager)"), this);
    title->setObjectName(QStringLiteral("PresetHubTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("快速切换多屏拓扑配置、一键另存或导出为 JSON 配置文件"), this);
    subtitle->setObjectName(QStringLiteral("PresetHubSubtitle"));
    titleCol->addWidget(title);
    titleCol->addWidget(subtitle);
    header->addLayout(titleCol, 1);

    auto *closeBtn = new PresetCloseButton(this);
    header->addWidget(closeBtn, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *saveCard = new QFrame(this);
    saveCard->setObjectName(QStringLiteral("PresetSaveCard"));
    auto *saveLay = new QHBoxLayout(saveCard);
    saveLay->setContentsMargins(16, 14, 16, 14);
    saveLay->setSpacing(12);

    auto *saveTextCol = new QVBoxLayout();
    saveTextCol->setSpacing(4);
    auto *saveTitle = new QLabel(QStringLiteral("保存当前运行状态为新方案"), this);
    saveTitle->setObjectName(QStringLiteral("PresetSaveTitle"));
    auto *saveSub = new QLabel(currentMountSummary(m_current), this);
    saveSub->setObjectName(QStringLiteral("PresetSaveSubtitle"));
    saveTextCol->addWidget(saveTitle);
    saveTextCol->addWidget(saveSub);
    saveLay->addLayout(saveTextCol, 1);

    auto *saveBtn = new QPushButton(QStringLiteral("另存为方案…"), this);
    saveBtn->setObjectName(QStringLiteral("PresetSaveBtn"));
    saveBtn->setIcon(svgIcon(QStringLiteral(":/icons/icon_save.svg"), QColor(0x06, 0xb6, 0xd4), 16));
    saveBtn->setIconSize(QSize(16, 16));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveLay->addWidget(saveBtn, 0, Qt::AlignVCenter);
    root->addWidget(saveCard);

    auto *sectionTitle = new QLabel(QStringLiteral("全部方案库"), this);
    sectionTitle->setObjectName(QStringLiteral("PresetSectionTitle"));
    root->addWidget(sectionTitle);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PresetScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *listHost = new QWidget(scroll);
    listHost->setObjectName(QStringLiteral("PresetListHost"));
    m_listLayout = new QVBoxLayout(listHost);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(10);
    m_listLayout->addStretch(1);
    scroll->setWidget(listHost);
    root->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout();
    footer->setSpacing(10);
    auto *exportBtn = new QPushButton(QStringLiteral("导出方案 JSON"), this);
    exportBtn->setObjectName(QStringLiteral("PresetExportBtn"));
    exportBtn->setIcon(
        svgIcon(QStringLiteral(":/icons/icon_export.svg"), QColor(0x06, 0xb6, 0xd4), 16));
    exportBtn->setIconSize(QSize(16, 16));
    exportBtn->setCursor(Qt::PointingHandCursor);
    footer->addWidget(exportBtn);

    footer->addStretch(1);

    auto *doneBtn = new QPushButton(QStringLiteral("完成并关闭"), this);
    doneBtn->setObjectName(QStringLiteral("PresetDoneBtn"));
    doneBtn->setCursor(Qt::PointingHandCursor);
    footer->addWidget(doneBtn);
    root->addLayout(footer);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(doneBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &PresetHubDialog::onSaveNew);
    connect(exportBtn, &QPushButton::clicked, this, &PresetHubDialog::onExportJson);

    rebuildList();
}

void PresetHubDialog::rebuildList()
{
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QStringList paths = listProfilePaths();
    if (paths.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无已存方案，可将当前运行状态另存为新方案"), this);
        empty->setObjectName(QStringLiteral("PresetEmptyHint"));
        empty->setAlignment(Qt::AlignCenter);
        m_listLayout->addWidget(empty);
        m_listLayout->addStretch(1);
        return;
    }

    for (const QString &path : paths) {
        const QString base = QFileInfo(path).completeBaseName();
        const AppConfig cfg = AppConfig::loadFromFile(path);
        const bool active = isActiveProfile(path, m_activePath, m_current, base);

        auto *card = new QFrame(this);
        card->setObjectName(active ? QStringLiteral("PresetProfileCardActive")
                                   : QStringLiteral("PresetProfileCard"));
        auto *cardLay = new QHBoxLayout(card);
        cardLay->setContentsMargins(16, 14, 16, 14);
        cardLay->setSpacing(14);

        auto *left = new QVBoxLayout();
        left->setSpacing(8);

        auto *titleRow = new QHBoxLayout();
        titleRow->setSpacing(8);
        auto *nameLab = new QLabel(base, card);
        nameLab->setObjectName(QStringLiteral("PresetProfileName"));
        titleRow->addWidget(nameLab);
        titleRow->addWidget(makePill(profileCategoryTag(cfg.displays.size()), "PresetTagPill", card));
        if (active)
            titleRow->addWidget(makePill(QStringLiteral("当前生效中"), "PresetActivePill", card));
        titleRow->addStretch(1);
        left->addLayout(titleRow);

        auto *desc = new QLabel(profileDescription(cfg), card);
        desc->setObjectName(QStringLiteral("PresetProfileDesc"));
        desc->setWordWrap(true);
        left->addWidget(desc);

        auto *pillRow = new QHBoxLayout();
        pillRow->setSpacing(8);
        for (int i = 0; i < cfg.displays.size(); ++i) {
            const DisplaySpec &d = cfg.displays[i];
            const QString label = cfg.displays.size() > 1
                                      ? QStringLiteral("屏 %1: %2x%3 (%4Hz)")
                                            .arg(i + 1)
                                            .arg(d.width)
                                            .arg(d.height)
                                            .arg(d.hz)
                                      : QStringLiteral("屏 1: %1x%2 (%3Hz)")
                                            .arg(d.width)
                                            .arg(d.height)
                                            .arg(d.hz);
            pillRow->addWidget(makePill(label, "PresetScreenPill", card));
        }
        pillRow->addStretch(1);
        left->addLayout(pillRow);
        cardLay->addLayout(left, 1);

        auto *actions = new QHBoxLayout();
        actions->setSpacing(8);
        actions->setAlignment(Qt::AlignVCenter);

        auto *applyBtn = new QPushButton(active ? QStringLiteral("重新应用")
                                                : QStringLiteral("一键应用"),
                                         card);
        applyBtn->setObjectName(active ? QStringLiteral("PresetReapplyBtn")
                                         : QStringLiteral("PresetApplyBtn"));
        applyBtn->setCursor(Qt::PointingHandCursor);
        applyBtn->setMinimumWidth(96);
        applyBtn->setFixedSize(applyBtn->minimumWidth(), 32);

        auto *delBtn = new QPushButton(card);
        delBtn->setObjectName(QStringLiteral("PresetDeleteBtn"));
        delBtn->setIcon(svgIcon(QStringLiteral(":/icons/icon_trash.svg"),
                                QColor(0xcb, 0xd5, 0xe1), 16));
        delBtn->setIconSize(QSize(16, 16));
        delBtn->setFixedSize(32, 32);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setToolTip(QStringLiteral("删除方案"));

        actions->addWidget(applyBtn, 0, Qt::AlignVCenter);
        actions->addWidget(delBtn, 0, Qt::AlignVCenter);

        cardLay->addLayout(actions);

        connect(applyBtn, &QPushButton::clicked, this, [this, path]() { onLoad(path); });
        connect(delBtn, &QPushButton::clicked, this, [this, path]() { onDeleteProfile(path); });

        m_listLayout->addWidget(card);
    }
    m_listLayout->addStretch(1);
}

void PresetHubDialog::onLoad(const QString &path)
{
    if (path.isEmpty())
        return;
    m_action = LoadProfile;
    m_selectedPath = path;
    accept();
}

void PresetHubDialog::onSaveNew()
{
    m_action = SaveAsNew;
    accept();
}

void PresetHubDialog::onDeleteProfile(const QString &path)
{
    const QString name = QFileInfo(path).completeBaseName();
    if (QMessageBox::question(this, QStringLiteral("删除方案"),
                              QStringLiteral("确定删除方案「%1」？").arg(name))
        != QMessageBox::Yes)
        return;

    QString err;
    if (!deleteProfileFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), err);
        return;
    }

    m_profilesChanged = true;
    if (!m_activePath.isEmpty()
        && QFileInfo(path).absoluteFilePath() == QFileInfo(m_activePath).absoluteFilePath())
        m_activePath.clear();
    rebuildList();
}

void PresetHubDialog::onExportJson()
{
    QString src = m_activePath;
    if (src.isEmpty() || !QFile::exists(src)) {
        const QStringList paths = listProfilePaths();
        if (!paths.isEmpty())
            src = paths.first();
    }

    const QString dst = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出方案 JSON"),
        QDir(profilesDir()).filePath(QFileInfo(src).fileName()),
        QStringLiteral("JSON (*.json)"));
    if (dst.isEmpty())
        return;

    if (!src.isEmpty() && QFile::exists(src)) {
        if (QFile::exists(dst))
            QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"),
                                 QStringLiteral("无法复制到 %1").arg(dst));
            return;
        }
    } else {
        QString err;
        if (!m_current.saveToFile(dst, &err)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"), err);
            return;
        }
    }
    m_profilesChanged = true;
    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("已导出至 %1").arg(dst));
}
