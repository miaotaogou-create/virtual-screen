#pragma once

#include "AppConfig.h"
#include "WinDisplay.h"

#include <QPixmap>
#include <QPoint>
#include <QVector>
#include <QWidget>

class TopologyCanvas : public QWidget
{
    Q_OBJECT
public:
    enum ViewMode { Focus, Topology, Grid };

    explicit TopologyCanvas(QWidget *parent = nullptr);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_mode; }

    void setSelectedIndex(int index);
    int selectedIndex() const { return m_selected; }

    void setDisplays(const QVector<DisplaySpec> &specs);
    void setVirtuals(const QVector<MonitorInfo> &virtuals);
    void setPreviewPixmap(int index, const QPixmap &pm);
    QPixmap previewPixmap(int index) const;
    /** 全景网格第 index 格预览区逻辑像素尺寸（不含 DPR）。 */
    QSize gridPreviewSize(int index) const;
    void setPlaceholderText(const QString &text);

signals:
    void displayClicked(int index);
    void expandRequested();
    void screenshotRequested();
    /** 拓扑拖拽结束：各屏在虚拟桌面上的新左上角坐标（与 displays 下标对齐）。 */
    void topologyArranged(const QVector<QPoint> &origins);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void paintFocus(QPainter &p);
    void paintGrid(QPainter &p);
    void paintTopology(QPainter &p);
    void paintFocusFrameTools(QPainter &p, const QRect &frame) const;
    void paintTopoCard(QPainter &p, int index, const QRect &r, bool selected) const;

    QRect focusToolRect(const QRect &frame, int which) const;
    int hitTestFocusTool(const QPoint &pos) const;
    QRect focusFrameRect(const DisplaySpec &spec) const;
    QRect gridCellRect(int index, int count) const;
    int hitTestDisplay(const QPoint &pos) const;

    QRect topologyPanelRect() const;
    void ensureTopoOrigins();
    void recomputeTopoFit();
    QRect topoCardRect(int index) const;
    QPoint worldToView(const QPoint &world) const;
    QPoint viewToWorld(const QPoint &view) const;
    QPoint snapWorld(const QPoint &world, int movingIndex) const;
    MonitorInfo monitorFor(int index) const;

    ViewMode m_mode = Focus;
    int m_selected = 0;
    int m_hoverTool = -1;
    int m_hoverCard = -1;
    int m_dragIndex = -1;
    QPoint m_dragGrabWorld;
    QPoint m_dragStartOrigin;
    bool m_snapEnabled = true;
    qreal m_topoScale = 0.12;
    QPoint m_topoViewOrigin;
    QVector<DisplaySpec> m_specs;
    QVector<MonitorInfo> m_virtuals;
    QVector<QPixmap> m_previews;
    QVector<QPoint> m_topoOrigins;
    QString m_placeholder;
};
