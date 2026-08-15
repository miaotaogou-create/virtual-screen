#include "CastWindowDialog.h"

#include "WinDisplay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

CastWindowDialog::CastWindowDialog(const QString &targetLabel, const MonitorInfo &targetMonitor,
                                   QWidget *parent)
    : QDialog(parent)
    , m_target(targetMonitor)
    , m_targetLabel(targetLabel)
{
    setWindowTitle(QStringLiteral("投放系统窗口到虚拟显示器"));
    resize(520, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("🚀 投放系统窗口到虚拟显示器"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px;font-weight:bold;color:#f8fafc;"));
    auto *hint = new QLabel(QStringLiteral("Win32 移动"), this);
    hint->setStyleSheet(QStringLiteral("color:#64748b;font-size:11px;"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(hint);
    layout->addLayout(header);

    layout->addWidget(new QLabel(
        QStringLiteral("目标：%1 · 位置 (%2, %3)")
            .arg(m_targetLabel)
            .arg(m_target.geometry.x())
            .arg(m_target.geometry.y()),
        this));

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索进程或窗口标题…"));
    layout->addWidget(m_search);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto *btns = new QHBoxLayout();
    btns->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("关闭"), this);
    auto *cast = new QPushButton(QStringLiteral("立即投放"), this);
    cast->setObjectName(QStringLiteral("PrimaryBtn"));
    btns->addWidget(cancel);
    btns->addWidget(cast);
    layout->addLayout(btns);

    connect(m_search, &QLineEdit::textChanged, this, &CastWindowDialog::refreshList);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &CastWindowDialog::castToTarget);
    connect(cast, &QPushButton::clicked, this, &CastWindowDialog::castToTarget);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    refreshList();
}

void CastWindowDialog::refreshList()
{
    m_list->clear();
    m_windows = WinDisplay::listTopWindows();
    const QString filter = m_search->text().trimmed().toLower();

    for (int i = 0; i < m_windows.size(); ++i) {
        const TopWindowInfo &w = m_windows[i];
        const QString blob = (w.processName + w.title).toLower();
        if (!filter.isEmpty() && !blob.contains(filter))
            continue;

        const QString line1 = w.title;
        const QString line2 = QStringLiteral("%1 · HWND: 0x%2")
                                  .arg(w.processName.isEmpty() ? QStringLiteral("unknown") : w.processName)
                                  .arg(w.hwnd, 0, 16);
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(line1, line2), m_list);
        item->setData(Qt::UserRole, i);
    }
}

void CastWindowDialog::castToTarget()
{
    auto *item = m_list->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("投放"), QStringLiteral("请先选择一个窗口。"));
        return;
    }
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_windows.size())
        return;

    const TopWindowInfo &w = m_windows[idx];
    if (!WinDisplay::moveWindowToMonitor(w.hwnd, m_target.geometry)) {
        QMessageBox::warning(this, QStringLiteral("投放失败"),
                             QStringLiteral("无法移动该窗口（可能已关闭或权限不足）。"));
        return;
    }
    m_selectedHwnd = w.hwnd;
    m_selectedTitle = w.title;
    m_selectedProcessName = w.processName.isEmpty() ? QStringLiteral("unknown") : w.processName;
    accept();
}
