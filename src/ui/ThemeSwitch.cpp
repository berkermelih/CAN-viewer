#include "ThemeSwitch.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;
} // namespace

ThemeSwitch::ThemeSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_animation(new QVariantAnimation(this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    m_animation->setDuration(180);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_position = value.toReal();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, &ThemeSwitch::AnimateTo);
}

QSize ThemeSwitch::sizeHint() const
{
    return QSize(58, 30);
}

void ThemeSwitch::AnimateTo(bool checked)
{
    m_animation->stop();
    m_animation->setStartValue(m_position);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    if (isVisible()) {
        m_animation->start();
    } else {
        m_position = checked ? 1.0 : 0.0;
        update();
    }
}

void ThemeSwitch::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ThemeSwitch::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ThemeSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const Theme::Palette &colors = Theme::Colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QSizeF size = sizeHint();
    const QRectF track((width() - size.width()) / 2.0 + 1, (height() - size.height()) / 2.0 + 1, size.width() - 2,
                       size.height() - 2);
    const qreal radius = track.height() / 2.0;

    // Ray: açıkta sıcak gri, koyuda vurgu tonu
    QColor trackColor = isChecked() ? colors.accentSoft : colors.border;
    if (m_hovered) {
        trackColor = trackColor.darker(Theme::CurrentMode() == Theme::Mode::Dark ? 80 : 108);
    }
    painter.setPen(QPen(m_hovered ? colors.accent : colors.border, 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, radius, radius);

    // Rayın boş tarafındaki silik simge (karşı mod)
    const qreal knobRadius = radius - 3;
    const QPointF leftCenter(track.left() + radius, track.center().y());
    const QPointF rightCenter(track.right() - radius, track.center().y());
    painter.setOpacity(0.45);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colors.textMuted);
    if (m_position < 0.5) {
        DrawMoon(painter, rightCenter, knobRadius * 0.85, trackColor);
    } else {
        DrawSun(painter, leftCenter, knobRadius * 0.85);
    }
    painter.setOpacity(1.0);

    // Topuz ve içindeki simge
    const QPointF center(leftCenter.x() + (rightCenter.x() - leftCenter.x()) * m_position, track.center().y());
    const QColor knobColor = m_position < 0.5 ? colors.base : colors.surface;
    painter.setPen(QPen(colors.border.darker(110), 1));
    painter.setBrush(knobColor);
    painter.drawEllipse(center, knobRadius, knobRadius);
    painter.setPen(Qt::NoPen);
    if (m_position < 0.5) {
        painter.setBrush(colors.warning);
        DrawSun(painter, center, knobRadius);
    } else {
        painter.setBrush(colors.accent);
        DrawMoon(painter, center, knobRadius, knobColor);
    }

    if (hasFocus()) {
        painter.setPen(QPen(colors.accent, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-1, -1, 1, 1), radius + 1, radius + 1);
    }
}

void ThemeSwitch::DrawSun(QPainter &painter, const QPointF &center, qreal radius)
{
    const QColor color = painter.brush().color().isValid() && painter.brush().style() != Qt::NoBrush
                             ? painter.brush().color()
                             : Theme::Colors().warning;
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, radius * 0.38, radius * 0.38);
    QPen rayPen(color, qMax(1.2, radius * 0.14));
    rayPen.setCapStyle(Qt::RoundCap);
    painter.setPen(rayPen);
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * Pi / 4.0;
        const QPointF direction(std::cos(angle), std::sin(angle));
        painter.drawLine(center + direction * radius * 0.58, center + direction * radius * 0.82);
    }
    painter.setPen(Qt::NoPen);
}

void ThemeSwitch::DrawMoon(QPainter &painter, const QPointF &center, qreal radius, const QColor &background)
{
    Q_UNUSED(background)
    // Hilal: büyük daireden kaydırılmış bir dairenin çıkarılmasıyla oluşur.
    const qreal r = radius * 0.62;
    QPainterPath disc;
    disc.addEllipse(center, r, r);
    QPainterPath cut;
    cut.addEllipse(center + QPointF(r * 0.55, -r * 0.45), r * 0.85, r * 0.85);
    painter.drawPath(disc.subtracted(cut));
}
