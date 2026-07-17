#include "ui/widgets/HealthRingWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <cmath>

namespace tp {

HealthRingWidget::HealthRingWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(80, 80);
}

void HealthRingWidget::setValue(float v) {
    m_value = std::max(0.0f, std::min(1.0f, v));
    update();
}

void HealthRingWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int side = std::min(width(), height());
    QRectF rect((width() - side) / 2.0 + 8,
                (height() - side) / 2.0 + 8,
                side - 16, side - 16);

    // Background arc
    p.setPen(QPen(QColor("#2A2A4A"), 10, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(rect, 225 * 16, -270 * 16);

    // Foreground arc
    QColor arcColor;
    if (m_value >= 0.6f)      arcColor = QColor("#00C853");
    else if (m_value >= 0.3f) arcColor = QColor("#FFD600");
    else                       arcColor = QColor("#FF1744");

    int span = static_cast<int>(-270.0f * m_value) * 16;
    p.setPen(QPen(arcColor, 10, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(rect, 225 * 16, span);

    // Text
    p.setPen(QColor("#E0E0E0"));
    p.setFont(QFont("Roboto", static_cast<int>(side * 0.18), QFont::Bold));
    p.drawText(rect, Qt::AlignCenter,
               QString::number(static_cast<int>(m_value * 100)) + "%");
}

} // namespace tp
