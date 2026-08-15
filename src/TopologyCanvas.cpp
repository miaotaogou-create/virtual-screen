#include "TopologyCanvas.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QtMath>

namespace {
constexpr int kTopoSnapPx = 80; // 虚拟桌面吸附步长
}

TopologyCanvas::TopologyCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(320);
}

void TopologyCanvas::setViewMode(ViewMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    if (m_mode == Topology) {
        ensureTopoOrigins();
        recomputeTopoFit();
    }
    update();
}

void TopologyCanvas::setSelectedIndex(int index)
{
    if (index == m_selected)
        return;
    m_selected = qBound(0, index, qMax(0, m_specs.size() - 1));
    update();
}

void TopologyCanvas::setDisplays(const QVector<DisplaySpec> &specs)
{
    m_specs = specs;
    m_previews.resize(specs.size());
    if (m_selected >= specs.size())
        m_selected = qMax(0, specs.size() - 1);
    ensureTopoOrigins();
    recomputeTopoFit();
    update();
}

void TopologyCanvas::setVirtuals(const QVector<MonitorInfo> &virtuals)
{
    m_virtuals = virtuals;
    // 若尚未拖过，用系统枚举坐标刷新原点
    if (m_dragIndex < 0) {
        ensureTopoOrigins();
        for (int i = 0; i < m_specs.size(); ++i) {
            if (i < m_virtuals.size() && !m_virtuals[i].deviceName.isEmpty())
                m_topoOrigins[i] = m_virtuals[i].geometry.topLeft();
        }
        recomputeTopoFit();
    }
    update();
}

void TopologyCanvas::setPreviewPixmap(int index, const QPixmap &pm)
{
    if (index < 0 || index >= m_previews.size())
        return;
    m_previews[index] = pm;
    update();
}

QPixmap TopologyCanvas::previewPixmap(int index) const
{
    if (index < 0 || index >= m_previews.size())
        return QPixmap();
    return m_previews[index];
}

QSize TopologyCanvas::gridPreviewSize(int index) const
{
    if (index < 0 || m_specs.isEmpty())
        return size() / 2;
    const int n = m_specs.size();
    const int i = qBound(0, index, n - 1);
    return gridCellRect(i, n).adjusted(8, 28, -8, -8).size();
}

void TopologyCanvas::setPlaceholderText(const QString &text)
{
    m_placeholder = text;
    update();
}

void TopologyCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_mode == Topology)
        recomputeTopoFit();
}

void TopologyCanvas::ensureTopoOrigins()
{
    const int n = m_specs.size();
    while (m_topoOrigins.size() < n) {
        const int i = m_topoOrigins.size();
        if (i < m_virtuals.size() && !m_virtuals[i].deviceName.isEmpty())
            m_topoOrigins.push_back(m_virtuals[i].geometry.topLeft());
        else
            m_topoOrigins.push_back(QPoint(i * (i < m_specs.size() ? m_specs[i].width : 1920), 0));
    }
    while (m_topoOrigins.size() > n)
        m_topoOrigins.removeLast();
}

MonitorInfo TopologyCanvas::monitorFor(int index) const
{
    MonitorInfo mon;
    if (index < 0 || index >= m_specs.size())
        return mon;
    if (index < m_virtuals.size() && !m_virtuals[index].deviceName.isEmpty()) {
        mon = m_virtuals[index];
        if (index < m_topoOrigins.size())
            mon.geometry.moveTopLeft(m_topoOrigins[index]);
        return mon;
    }
    mon.geometry = QRect(m_topoOrigins.value(index),
                         QSize(m_specs[index].width, m_specs[index].height));
    mon.primary = (index == 0);
    return mon;
}

QRect TopologyCanvas::topologyPanelRect() const
{
    return rect().adjusted(16, 44, -16, -16);
}

void TopologyCanvas::recomputeTopoFit()
{
    ensureTopoOrigins();
    const QRect panel = topologyPanelRect().adjusted(28, 28, -28, -28);
    if (panel.width() < 40 || panel.height() < 40 || m_specs.isEmpty()) {
        m_topoScale = 0.12;
        m_topoViewOrigin = panel.topLeft();
        return;
    }

    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    for (int i = 0; i < m_specs.size(); ++i) {
        const QPoint o = m_topoOrigins[i];
        const int w = m_specs[i].width;
        const int h = m_specs[i].height;
        if (i == 0) {
            minX = o.x();
            minY = o.y();
            maxX = o.x() + w;
            maxY = o.y() + h;
        } else {
            minX = qMin(minX, o.x());
            minY = qMin(minY, o.y());
            maxX = qMax(maxX, o.x() + w);
            maxY = qMax(maxY, o.y() + h);
        }
    }
    const qreal worldW = qMax(1, maxX - minX);
    const qreal worldH = qMax(1, maxY - minY);
    const qreal sx = panel.width() / worldW;
    const qreal sy = panel.height() / worldH;
    m_topoScale = qBound(0.06, qMin(sx, sy) * 0.88, 0.22);

    const int drawnW = int(worldW * m_topoScale);
    const int drawnH = int(worldH * m_topoScale);
    m_topoViewOrigin = QPoint(panel.left() + (panel.width() - drawnW) / 2 - int(minX * m_topoScale),
                              panel.top() + (panel.height() - drawnH) / 2 - int(minY * m_topoScale));
}

QPoint TopologyCanvas::worldToView(const QPoint &world) const
{
    return QPoint(m_topoViewOrigin.x() + int(world.x() * m_topoScale),
                  m_topoViewOrigin.y() + int(world.y() * m_topoScale));
}

QPoint TopologyCanvas::viewToWorld(const QPoint &view) const
{
    return QPoint(int((view.x() - m_topoViewOrigin.x()) / m_topoScale),
                  int((view.y() - m_topoViewOrigin.y()) / m_topoScale));
}

QPoint TopologyCanvas::snapWorld(const QPoint &world, int movingIndex) const
{
    QPoint out = world;
    if (m_snapEnabled) {
        auto snap = [](int v) {
            return ((v + kTopoSnapPx / 2) / kTopoSnapPx) * kTopoSnapPx;
        };
        out = QPoint(snap(world.x()), snap(world.y()));
    }

    if (movingIndex < 0 || movingIndex >= m_specs.size())
        return out;

    const int mw = m_specs[movingIndex].width;
    const int mh = m_specs[movingIndex].height;
    const int thresh = 48;
    int bestX = out.x();
    int bestY = out.y();
    int bestDist = thresh + 1;

    auto trySnap = [&](int nx, int ny) {
        const int d = qAbs(nx - out.x()) + qAbs(ny - out.y());
        if (d <= thresh && d < bestDist) {
            bestDist = d;
            bestX = nx;
            bestY = ny;
        }
    };

    for (int i = 0; i < m_specs.size(); ++i) {
        if (i == movingIndex)
            continue;
        const int ox = m_topoOrigins[i].x();
        const int oy = m_topoOrigins[i].y();
        const int ow = m_specs[i].width;
        const int oh = m_specs[i].height;

        // 左右拼接
        trySnap(ox + ow, out.y());
        trySnap(ox - mw, out.y());
        trySnap(ox + ow, oy);
        trySnap(ox - mw, oy);
        // 上下拼接
        trySnap(out.x(), oy + oh);
        trySnap(out.x(), oy - mh);
        trySnap(ox, oy + oh);
        trySnap(ox, oy - mh);
        // 边对齐
        trySnap(ox, out.y());
        trySnap(ox + ow - mw, out.y());
        trySnap(out.x(), oy);
        trySnap(out.x(), oy + oh - mh);
    }

    if (bestDist <= thresh)
        return QPoint(bestX, bestY);
    return out;
}

QRect TopologyCanvas::topoCardRect(int index) const
{
    if (index < 0 || index >= m_specs.size())
        return QRect();
    const QPoint tl = worldToView(m_topoOrigins[index]);
    const int w = qMax(160, int(m_specs[index].width * m_topoScale));
    const int h = qMax(100, int(m_specs[index].height * m_topoScale));
    return QRect(tl, QSize(w, h));
}

QRect TopologyCanvas::focusFrameRect(const DisplaySpec &spec) const
{
    const int margin = 24;
    int frameW = qMax(200, width() - margin * 2);
    int frameH = frameW * spec.height / qMax(1, spec.width);
    const int maxH = height() - margin * 2;
    if (frameH > maxH) {
        frameH = maxH;
        frameW = frameH * spec.width / qMax(1, spec.height);
    }
    return QRect((width() - frameW) / 2, (height() - frameH) / 2, frameW, frameH);
}

QRect TopologyCanvas::gridCellRect(int index, int count) const
{
    const int cols = count <= 1 ? 1 : (count <= 4 ? 2 : 3);
    const int rows = (count + cols - 1) / cols;
    const int pad = 16;
    const int gap = 12;
    const int availW = width() - pad * 2;
    const int availH = height() - pad * 2;
    const int slotW = (availW - gap * (cols - 1)) / cols;
    const int slotH = (availH - gap * (rows - 1)) / rows;
    const int col = index % cols;
    const int row = index / cols;
    const QRect slot(pad + col * (slotW + gap), pad + row * (slotH + gap), slotW, slotH);

    // 卡片按该屏真实宽高比塞进格子，避免竖格里整屏上下留黑边、又不必裁切界面
    qreal aspect = 16.0 / 10.0;
    if (index >= 0 && index < m_specs.size() && m_specs[index].height > 0)
        aspect = qreal(m_specs[index].width) / qreal(m_specs[index].height);

    QSize card(slotW, qRound(slotW / aspect));
    if (card.height() > slotH) {
        card.setHeight(slotH);
        card.setWidth(qRound(slotH * aspect));
    }
    return QRect(slot.x() + (slotW - card.width()) / 2,
                 slot.y() + (slotH - card.height()) / 2,
                 card.width(), card.height());
}

QRect TopologyCanvas::focusToolRect(const QRect &frame, int which) const
{
    const int btn = 28;
    const int gap = 6;
    const int x0 = frame.right() - 12 - btn * 2 - gap;
    const int y0 = frame.top() + 12;
    return QRect(x0 + which * (btn + gap), y0, btn, btn);
}

int TopologyCanvas::hitTestFocusTool(const QPoint &pos) const
{
    if (m_mode != Focus || m_specs.isEmpty())
        return -1;
    const int idx = qBound(0, m_selected, m_specs.size() - 1);
    const QRect frame = focusFrameRect(m_specs[idx]);
    for (int i = 0; i < 2; ++i) {
        if (focusToolRect(frame, i).contains(pos))
            return i;
    }
    return -1;
}

int TopologyCanvas::hitTestDisplay(const QPoint &pos) const
{
    if (m_mode == Focus) {
        if (m_specs.isEmpty())
            return -1;
        const int idx = qBound(0, m_selected, m_specs.size() - 1);
        if (focusFrameRect(m_specs[idx]).contains(pos))
            return idx;
        return -1;
    }
    if (m_mode == Grid) {
        for (int i = 0; i < m_specs.size(); ++i) {
            if (gridCellRect(i, m_specs.size()).contains(pos))
                return i;
        }
        return -1;
    }
    // Topology：后绘制的优先（叠在上面）
    for (int i = m_specs.size() - 1; i >= 0; --i) {
        if (topoCardRect(i).contains(pos))
            return i;
    }
    return -1;
}

void TopologyCanvas::paintFocusFrameTools(QPainter &p, const QRect &frame) const
{
    for (int i = 0; i < 2; ++i) {
        const QRect r = focusToolRect(frame, i);
        const bool hover = (i == m_hoverTool);
        p.setPen(QPen(QColor(255, 255, 255, hover ? 70 : 35), 1));
        p.setBrush(QColor(0, 0, 0, hover ? 190 : 140));
        p.drawRoundedRect(r, 6, 6);

        p.setPen(QPen(QColor(hover ? QStringLiteral("#ffffff") : QStringLiteral("#cbd5e1")),
                      1.6, Qt::SolidLine, Qt::RoundCap));
        if (i == 0) {
            const int m = 8;
            p.drawLine(r.left() + m, r.top() + m + 4, r.left() + m, r.top() + m);
            p.drawLine(r.left() + m, r.top() + m, r.left() + m + 4, r.top() + m);
            p.drawLine(r.right() - m, r.top() + m + 4, r.right() - m, r.top() + m);
            p.drawLine(r.right() - m, r.top() + m, r.right() - m - 4, r.top() + m);
            p.drawLine(r.left() + m, r.bottom() - m - 4, r.left() + m, r.bottom() - m);
            p.drawLine(r.left() + m, r.bottom() - m, r.left() + m + 4, r.bottom() - m);
            p.drawLine(r.right() - m, r.bottom() - m - 4, r.right() - m, r.bottom() - m);
            p.drawLine(r.right() - m, r.bottom() - m, r.right() - m - 4, r.bottom() - m);
        } else {
            const QRect cam = r.adjusted(7, 8, -7, -8);
            p.drawRoundedRect(cam, 2, 2);
            p.drawEllipse(cam.center(), 3, 3);
        }
    }
}

void TopologyCanvas::paintTopoCard(QPainter &p, int index, const QRect &r, bool selected) const
{
    const DisplaySpec &spec = m_specs[index];
    const QPoint origin = m_topoOrigins.value(index);
    const bool primary = (index == 0);
    const bool hover = (index == m_hoverCard) || (index == m_dragIndex);

    // 卡片底
    QLinearGradient fill(r.topLeft(), r.bottomLeft());
    fill.setColorAt(0.0, QColor(selected ? 0x0f2233 : 0x0c1524));
    fill.setColorAt(1.0, QColor(selected ? 0x0a1a2a : 0x09111d));
    p.setBrush(fill);
    p.setPen(QPen(selected ? QColor(QStringLiteral("#06b6d4"))
                           : (hover ? QColor(QStringLiteral("#475569"))
                                    : QColor(QStringLiteral("#334155"))),
                  selected ? 2.2 : 1.4));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    // 选中微光
    if (selected) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(6, 182, 212, 22));
        p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 8, 8);
    }

    // 中央淡显示器图标
    {
        const int iw = qMin(56, r.width() / 3);
        const int ih = int(iw * 0.62);
        const QRect icon((r.width() - iw) / 2 + r.left(),
                         (r.height() - ih) / 2 + r.top() - 4, iw, ih);
        p.setPen(QPen(QColor(148, 163, 184, selected ? 70 : 40), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(icon, 4, 4);
        p.drawLine(icon.center().x() - iw / 6, icon.bottom() + 4,
                   icon.center().x() + iw / 6, icon.bottom() + 4);
        p.drawLine(icon.center().x(), icon.bottom() + 4,
                   icon.center().x(), icon.bottom() + 8);
    }

    // 标题
    p.setPen(QColor(QStringLiteral("#f8fafc")));
    QFont title = p.font();
    title.setBold(true);
    title.setPointSizeF(qMax(9.0, title.pointSizeF()));
    p.setFont(title);
    const QString role = primary ? QStringLiteral("主屏") : QStringLiteral("副屏");
    p.drawText(r.adjusted(14, 12, -14, 0), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("%1. %2 (%3)").arg(index + 1).arg(spec.label).arg(role));

    // 右上角徽章
    {
        const QString badge = primary
            ? QStringLiteral("Primary (X:%1, Y:%2)").arg(origin.x()).arg(origin.y())
            : QStringLiteral("Extended (X:%1, Y:%2)").arg(origin.x()).arg(origin.y());
        QFont bf = p.font();
        bf.setBold(false);
        bf.setPointSizeF(qMax(8.0, bf.pointSizeF() - 1));
        p.setFont(bf);
        const QFontMetrics fm(bf);
        const int bw = fm.horizontalAdvance(badge) + 16;
        const int bh = fm.height() + 6;
        const QRect br(r.right() - 14 - bw, r.top() + 12, bw, bh);
        p.setPen(Qt::NoPen);
        p.setBrush(primary ? QColor(6, 182, 212, 40) : QColor(30, 41, 59, 200));
        p.drawRoundedRect(br, 8, 8);
        if (primary) {
            p.setPen(QPen(QColor(QStringLiteral("#06b6d4")), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(br).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }
        p.setPen(primary ? QColor(QStringLiteral("#67e8f9")) : QColor(QStringLiteral("#94a3b8")));
        p.drawText(br, Qt::AlignCenter, badge);
    }

    // 底部规格
    p.setFont(QFont());
    p.setPen(QColor(QStringLiteral("#94a3b8")));
    p.drawText(r.adjusted(14, 0, -14, -12), Qt::AlignLeft | Qt::AlignBottom,
               QStringLiteral("%1 x %2 · %3Hz · %4%")
                   .arg(spec.width)
                   .arg(spec.height)
                   .arg(spec.hz)
                   .arg(spec.scale));
}

void TopologyCanvas::paintTopology(QPainter &p)
{
    // 外框面板
    const QRect panel = topologyPanelRect();
    p.setPen(QPen(QColor(QStringLiteral("#1e293b")), 1));
    p.setBrush(QColor(QStringLiteral("#070e18")));
    p.drawRoundedRect(QRectF(panel).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    // 顶栏标题
    p.setPen(QColor(QStringLiteral("#f8fafc")));
    QFont title = p.font();
    title.setBold(true);
    p.setFont(title);
    p.drawText(QRect(20, 12, width() - 200, 24), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("多显示器空间相对拓扑排列"));
    p.setFont(QFont());
    p.setPen(QColor(QStringLiteral("#67e8f9")));
    const QFontMetrics fm(p.font());
    const int titleW = QFontMetrics(title).horizontalAdvance(QStringLiteral("多显示器空间相对拓扑排列"));
    p.drawText(QRect(24 + titleW, 12, 360, 24), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("（支持自由拖拽对齐与坐标拼接）"));

    p.setPen(QColor(QStringLiteral("#22d3ee")));
    p.drawText(QRect(width() - 200, 12, 180, 24), Qt::AlignRight | Qt::AlignVCenter,
               m_snapEnabled ? QStringLiteral("吸附对齐网格：开启")
                             : QStringLiteral("吸附对齐网格：关闭"));

    // 面板内点阵网格
    p.setClipRect(panel.adjusted(1, 1, -1, -1));
    p.setPen(QColor(30, 41, 59, 140));
    const int step = 22;
    for (int x = panel.left() + 12; x < panel.right(); x += step) {
        for (int y = panel.top() + 12; y < panel.bottom(); y += step)
            p.drawPoint(x, y);
    }

    if (m_specs.isEmpty()) {
        p.setClipping(false);
        p.setPen(QColor(QStringLiteral("#64748b")));
        p.drawText(panel, Qt::AlignCenter, QStringLiteral("暂无虚拟屏，请先添加显示"));
        return;
    }

    // 先画未选中，再画选中，保证选中在上
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_specs.size(); ++i) {
            const bool sel = (i == m_selected);
            if (pass == 0 && sel)
                continue;
            if (pass == 1 && !sel)
                continue;
            paintTopoCard(p, i, topoCardRect(i), sel);
        }
    }
    p.setClipping(false);
}

void TopologyCanvas::paintFocus(QPainter &p)
{
    const int idx = qBound(0, m_selected, m_specs.size() - 1);
    const DisplaySpec &spec = m_specs[idx];
    const QRect frame = focusFrameRect(spec);

    if (!m_previews.value(idx).isNull()) {
        p.drawPixmap(frame, m_previews[idx].scaled(frame.size(), Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
    } else {
        p.fillRect(frame, QColor(QStringLiteral("#0f172a")));
        p.setPen(QColor(QStringLiteral("#475569")));
        p.drawText(frame, Qt::AlignCenter, QStringLiteral("预览加载中…"));
    }

    p.setPen(QPen(QColor(QStringLiteral("#0284c7")), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(frame.adjusted(-1, -1, 1, 1), 10, 10);
    // 全屏/截图已挪到信息栏，不再叠在预览画面上
}

void TopologyCanvas::paintGrid(QPainter &p)
{
    // ponytail: 不画顶部说明文案——双卡中间缝会透字，且与工具栏「全景网格」重复
    for (int i = 0; i < m_specs.size(); ++i) {
        const QRect cell = gridCellRect(i, m_specs.size());
        const bool sel = (i == m_selected);
        p.setPen(QPen(sel ? QColor(QStringLiteral("#06b6d4")) : QColor(QStringLiteral("#334155")),
                 sel ? 2.5 : 1.5));
        p.setBrush(QColor(QStringLiteral("#111827")));
        p.drawRoundedRect(cell, 8, 8);

        const QRect inner = cell.adjusted(8, 28, -8, -8);
        if (!m_previews.value(i).isNull()) {
            const QPixmap &pm = m_previews[i];
            const qreal dpr = qMax<qreal>(1.0, pm.devicePixelRatio());
            QSizeF logical(pm.width() / dpr, pm.height() / dpr);
            logical.scale(inner.size(), Qt::KeepAspectRatio);
            const QRectF dest(inner.x() + (inner.width() - logical.width()) / 2.0,
                              inner.y() + (inner.height() - logical.height()) / 2.0,
                              logical.width(), logical.height());
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(dest, pm, QRectF(0, 0, pm.width(), pm.height()));
        } else {
            p.fillRect(inner, QColor(QStringLiteral("#0f172a")));
        }

        p.setPen(sel ? QColor(QStringLiteral("#ffffff")) : QColor(QStringLiteral("#cbd5e1")));
        p.drawText(cell.adjusted(10, 6, -10, 0), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("%1 · %2×%3")
                       .arg(m_specs[i].label)
                       .arg(m_specs[i].width)
                       .arg(m_specs[i].height));
    }
}

void TopologyCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    p.fillRect(rect(), QColor(QStringLiteral("#050b14")));

    if (m_specs.isEmpty() && m_mode != Topology) {
        p.setPen(QColor(QStringLiteral("#64748b")));
        p.drawText(rect(), Qt::AlignCenter,
                   m_placeholder.isEmpty()
                       ? QStringLiteral("当前无活跃虚拟显示器\n点击「+ 添加显示」或从方案预设加载")
                       : m_placeholder);
        return;
    }

    if (m_mode == Focus) {
        if (m_specs.isEmpty()) {
            p.setPen(QColor(QStringLiteral("#64748b")));
            p.drawText(rect(), Qt::AlignCenter,
                       m_placeholder.isEmpty()
                           ? QStringLiteral("当前无活跃虚拟显示器\n点击「+ 添加显示」或从方案预设加载")
                           : m_placeholder);
            return;
        }
        paintFocus(p);
        return;
    }
    if (m_mode == Grid) {
        if (m_specs.isEmpty()) {
            p.setPen(QColor(QStringLiteral("#64748b")));
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无虚拟屏"));
            return;
        }
        paintGrid(p);
        return;
    }
    paintTopology(p);
}

void TopologyCanvas::mousePressEvent(QMouseEvent *event)
{
    const int hit = hitTestDisplay(event->pos());
    if (hit >= 0) {
        m_selected = hit;
        emit displayClicked(hit);
        if (m_mode == Topology && event->button() == Qt::LeftButton) {
            m_dragIndex = hit;
            m_dragStartOrigin = m_topoOrigins[hit];
            m_dragGrabWorld = viewToWorld(event->pos()) - m_topoOrigins[hit];
            setCursor(Qt::ClosedHandCursor);
        }
        update();
    }
    QWidget::mousePressEvent(event);
}

void TopologyCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mode == Topology && m_dragIndex >= 0) {
        QPoint next = viewToWorld(event->pos()) - m_dragGrabWorld;
        next = snapWorld(next, m_dragIndex);
        if (next != m_topoOrigins[m_dragIndex]) {
            m_topoOrigins[m_dragIndex] = next;
            // 拖动中不重算缩放，否则卡片会「吸回」原布局观感
            update();
        }
        return;
    }

    if (m_mode == Topology) {
        const int card = hitTestDisplay(event->pos());
        if (card != m_hoverCard) {
            m_hoverCard = card;
            setCursor(card >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void TopologyCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragIndex >= 0) {
        const int idx = m_dragIndex;
        m_dragIndex = -1;
        setCursor(m_hoverCard >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);

        // 松手再吸附一次（贴边）
        m_topoOrigins[idx] = snapWorld(m_topoOrigins[idx], idx);

        if (m_topoOrigins[idx] != m_dragStartOrigin)
            emit topologyArranged(m_topoOrigins);

        recomputeTopoFit();
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void TopologyCanvas::leaveEvent(QEvent *event)
{
    if (m_hoverCard >= 0) {
        m_hoverCard = -1;
        if (m_dragIndex < 0)
            setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::leaveEvent(event);
}
