#include "PreviewPane.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QHBoxLayout>

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    m_placeholder = QStringLiteral("暂无预览");

    m_guide = new QWidget(this);
    m_guide->setStyleSheet(QStringLiteral(
        "QWidget#guideCard {"
        "  background:#111827; border:1px solid #334155;"
        "}"
        "QLabel#guideTitle { color:#2DD4BF; font-size:15px; font-weight:600; background:transparent; }"
        "QLabel#guideBody { color:#CBD5E1; font-size:13px; background:transparent; }"
        "QPushButton {"
        "  padding:8px 18px; background:#1E293B; border:1px solid #334155; color:#E2E8F0;"
        "}"
        "QPushButton:hover { background:#334155; }"
        "QPushButton#primaryBtn {"
        "  background:#0F766E; border:1px solid #0D9488; color:#fff; font-weight:600;"
        "}"
        "QPushButton#primaryBtn:hover { background:#0D9488; }"
        "QPushButton:disabled { color:#64748B; background:#0F172A; }"));
    m_guide->setObjectName(QStringLiteral("guideCard"));

    auto *lay = new QVBoxLayout(m_guide);
    lay->setContentsMargins(28, 24, 28, 24);
    lay->setSpacing(14);
    m_guideTitle = new QLabel(m_guide);
    m_guideTitle->setObjectName(QStringLiteral("guideTitle"));
    m_guideTitle->setWordWrap(true);
    m_guideBody = new QLabel(m_guide);
    m_guideBody->setObjectName(QStringLiteral("guideBody"));
    m_guideBody->setWordWrap(true);
    m_guideBody->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    auto *btns = new QHBoxLayout();
    btns->setSpacing(10);
    m_primary = new QPushButton(m_guide);
    m_primary->setObjectName(QStringLiteral("primaryBtn"));
    m_primary->setCursor(Qt::PointingHandCursor);
    m_secondary = new QPushButton(m_guide);
    m_secondary->setCursor(Qt::PointingHandCursor);
    btns->addWidget(m_primary);
    btns->addWidget(m_secondary);
    btns->addStretch();
    lay->addWidget(m_guideTitle);
    lay->addWidget(m_guideBody);
    lay->addLayout(btns);
    m_guide->hide();

    connect(m_primary, &QPushButton::clicked, this, &PreviewPane::primaryClicked);
    connect(m_secondary, &QPushButton::clicked, this, &PreviewPane::secondaryClicked);
}

void PreviewPane::showGuidePanel(bool on)
{
    m_guide->setVisible(on);
    if (on)
        layoutGuide();
}

void PreviewPane::layoutGuide()
{
    if (!m_guide->isVisible())
        return;
    m_guide->adjustSize();
    const int w = qMin(520, qMax(360, width() - 80));
    m_guide->setFixedWidth(w);
    m_guide->adjustSize();
    const int x = (width() - m_guide->width()) / 2;
    const int y = (height() - m_guide->height()) / 2;
    m_guide->move(qMax(0, x), qMax(0, y));
}

void PreviewPane::setPixmap(const QPixmap &pm)
{
    m_source = pm;
    m_scaled = QPixmap();
    if (!pm.isNull()) {
        m_placeholder.clear();
        showGuidePanel(false);
        setCursor(Qt::CrossCursor);
    }
    update();
}

void PreviewPane::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    m_source = QPixmap();
    m_scaled = QPixmap();
    setCursor(Qt::ArrowCursor);
    showGuidePanel(false);
    update();
}

void PreviewPane::setGuide(const QString &title, const QString &body,
                           const QString &primaryText, const QString &secondaryText)
{
    m_source = QPixmap();
    m_scaled = QPixmap();
    m_placeholder.clear();
    setCursor(Qt::ArrowCursor);
    m_guideTitle->setText(title);
    m_guideBody->setText(body);
    m_primary->setText(primaryText);
    m_primary->setVisible(!primaryText.isEmpty());
    m_secondary->setText(secondaryText);
    m_secondary->setVisible(!secondaryText.isEmpty());
    showGuidePanel(true);
    update();
}

void PreviewPane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_scaled = QPixmap();
    layoutGuide();
}

void PreviewPane::ensureScaled()
{
    if (m_source.isNull() || size().width() < 2 || size().height() < 2)
        return;
    if (!m_scaled.isNull())
        return;
    const QSize fitted = m_source.size().scaled(size(), Qt::KeepAspectRatio);
    if (m_source.size() == fitted) {
        m_scaled = m_source;
        return;
    }
    m_scaled = m_source.scaled(fitted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QRect PreviewPane::contentRect() const
{
    if (m_source.isNull())
        return {};
    const_cast<PreviewPane *>(this)->ensureScaled();
    if (m_scaled.isNull())
        return {};
    const int x = (width() - m_scaled.width()) / 2;
    const int y = (height() - m_scaled.height()) / 2;
    return QRect(x, y, m_scaled.width(), m_scaled.height());
}

bool PreviewPane::mapToNorm(const QPoint &pos, qreal *nx, qreal *ny) const
{
    const QRect r = contentRect();
    if (r.width() < 2 || r.height() < 2 || !r.contains(pos))
        return false;
    *nx = qreal(pos.x() - r.x()) / qreal(r.width() - 1);
    *ny = qreal(pos.y() - r.y()) / qreal(r.height() - 1);
    *nx = qBound(0.0, *nx, 1.0);
    *ny = qBound(0.0, *ny, 1.0);
    return true;
}

void PreviewPane::emitPointer(const QPoint &pos, Qt::MouseButton button, bool pressed, int wheelDelta)
{
    qreal nx = 0, ny = 0;
    if (!mapToNorm(pos, &nx, &ny))
        return;
    emit pointerEvent(nx, ny, button, pressed, wheelDelta);
}

void PreviewPane::mousePressEvent(QMouseEvent *e)
{
    if (m_source.isNull()) {
        QWidget::mousePressEvent(e);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    grabMouse();
    m_dragging = true;
    emitPointer(e->pos(), e->button(), true);
    e->accept();
}

void PreviewPane::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_source.isNull()) {
        QWidget::mouseReleaseEvent(e);
        return;
    }
    emitPointer(e->pos(), e->button(), false);
    m_dragging = false;
    if (mouseGrabber() == this)
        releaseMouse();
    e->accept();
}

void PreviewPane::mouseMoveEvent(QMouseEvent *e)
{
    if (m_source.isNull()) {
        QWidget::mouseMoveEvent(e);
        return;
    }
    // 仅按下拖拽时转发移动，避免 SendInput 把光标打到虚拟屏后丢跟踪
    if (m_dragging || (e->buttons() != Qt::NoButton))
        emitPointer(e->pos(), Qt::NoButton, true);
    e->accept();
}

void PreviewPane::wheelEvent(QWheelEvent *e)
{
    if (m_source.isNull()) {
        QWidget::wheelEvent(e);
        return;
    }
    emitPointer(e->pos(), Qt::NoButton, false, e->angleDelta().y());
    e->accept();
}

void PreviewPane::keyPressEvent(QKeyEvent *e)
{
    if (m_source.isNull() || e->isAutoRepeat()) {
        QWidget::keyPressEvent(e);
        return;
    }
    emit keyEvent(e->key(), e->modifiers(), true);
    e->accept();
}

void PreviewPane::keyReleaseEvent(QKeyEvent *e)
{
    if (m_source.isNull() || e->isAutoRepeat()) {
        QWidget::keyReleaseEvent(e);
        return;
    }
    emit keyEvent(e->key(), e->modifiers(), false);
    e->accept();
}

void PreviewPane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0B, 0x12, 0x20));
    if (!m_source.isNull()) {
        ensureScaled();
        if (!m_scaled.isNull()) {
            const int x = (width() - m_scaled.width()) / 2;
            const int y = (height() - m_scaled.height()) / 2;
            p.drawPixmap(x, y, m_scaled);
        }
        return;
    }
    if (m_guide->isVisible())
        return;
    if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(0x94, 0xA3, 0xB8));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
    }
}
