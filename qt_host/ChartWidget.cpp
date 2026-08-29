#include "ChartWidget.h"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(280);
}

void ChartWidget::setSeries(const QVector<qint64> &times, const QVector<double> &values)
{
    m_times = times;
    m_values = values;
    update();
}

void ChartWidget::setRange(double minY, double maxY)
{
    m_minY = minY;
    m_maxY = maxY;
    update();
}

void ChartWidget::setThreshold(double thresholdY)
{
    m_hasThreshold = true;
    m_thresholdY = thresholdY;
    update();
}

void ChartWidget::clearThreshold()
{
    m_hasThreshold = false;
    m_thresholdY = 0.0;
    update();
}

void ChartWidget::setMetric(const QString &title, const QString &unit, const QColor &color)
{
    m_title = title;
    m_unit = unit;
    m_color = color;
    update();
}

void ChartWidget::clearSeries()
{
    m_times.clear();
    m_values.clear();
    update();
}

QSize ChartWidget::sizeHint() const
{
    return QSize(640, 300);
}

void ChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(8, 18, 28));

    const QRect plot = rect().adjusted(58, 18, -16, -34);
    if (plot.width() < 80 || plot.height() < 60)
        return;

    const QColor gridColor(46, 68, 86);
    const QColor textColor(140, 166, 180);
    painter.setPen(gridColor);
    painter.drawRect(plot.adjusted(-1, -1, 1, 1));

    for (int i = 0; i <= 4; ++i)
    {
        const int x = plot.left() + plot.width() * i / 4;
        painter.setPen(gridColor);
        painter.drawLine(x, plot.top(), x, plot.bottom());

        const int y = plot.bottom() - plot.height() * i / 4;
        painter.drawLine(plot.left(), y, plot.right(), y);

        const double value = m_minY + (m_maxY - m_minY) * i / 4.0;
        painter.setPen(textColor);
        painter.drawText(QRect(6, y - 9, plot.left() - 10, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', 1));
    }

    if (m_hasThreshold)
    {
        const double fraction = (m_thresholdY - m_minY) / (m_maxY - m_minY);
        const int y = plot.bottom() - qRound(plot.height() * fraction);
        painter.save();
        QPen dashPen(QColor(225, 29, 72), 1, Qt::DashLine);
        painter.setPen(dashPen);
        painter.drawLine(plot.left(), y, plot.right(), y);
        painter.setPen(QColor(255, 170, 180));
        painter.drawText(QRect(plot.left() + 6, y - 16, plot.width() - 12, 14),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString("threshold %1 %2").arg(m_thresholdY, 0, 'f', 2).arg(m_unit));
        painter.restore();
    }

    painter.setPen(textColor);
    painter.drawText(QRect(6, 2, plot.width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
                     m_title.isEmpty() ? QStringLiteral("History") : m_title);

    const qint64 nowMs = m_times.isEmpty() ? QDateTime::currentMSecsSinceEpoch() : m_times.last();
    for (int i = 0; i <= 2; ++i)
    {
        const qint64 labelMs = nowMs - static_cast<qint64>(m_windowSeconds - i * m_windowSeconds / 2) * 1000;
        const double secondsAgo = static_cast<double>(nowMs - labelMs) / 1000.0;
        const int x = plot.right() - qRound(plot.width() * secondsAgo / m_windowSeconds);
        painter.setPen(gridColor);
        painter.drawLine(x, plot.top(), x, plot.bottom());
        painter.setPen(textColor);
        painter.drawText(QRect(x - 45, plot.bottom() + 8, 90, 16),
                         Qt::AlignCenter,
                         QDateTime::fromMSecsSinceEpoch(labelMs).toString(QStringLiteral("HH:mm:ss")));
    }

    if (m_times.size() < 1)
    {
        painter.setPen(textColor);
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("Waiting for serial data..."));
        return;
    }

    const qint64 lastMs = m_times.last();
    QPainterPath path;
    bool pathStarted = false;
    const double lastValue = m_values.last();

    for (int i = 0; i < m_times.size(); ++i)
    {
        const double secondsAgo = static_cast<double>(lastMs - m_times.at(i)) / 1000.0;
        if (secondsAgo > m_windowSeconds)
            continue;

        const double fraction = (m_values.at(i) - m_minY) / (m_maxY - m_minY);
        const double clamped = qBound(0.0, fraction, 1.0);
        const int x = plot.right() - qRound(plot.width() * secondsAgo / m_windowSeconds);
        const int y = plot.bottom() - qRound(plot.height() * clamped);

        if (!pathStarted)
        {
            path.moveTo(x, y);
            pathStarted = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    painter.save();
    QPen linePen(m_color, 2);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);
    if (pathStarted)
        painter.drawPath(path);

    const double lastFraction = (lastValue - m_minY) / (m_maxY - m_minY);
    const int lastX = plot.right();
    const int lastY = plot.bottom() - qRound(plot.height() * qBound(0.0, lastFraction, 1.0));
    painter.setBrush(m_color);
    painter.drawEllipse(QPointF(lastX, lastY), 4.0, 4.0);

    painter.setPen(QColor(244, 248, 250));
    painter.drawText(QRect(lastX - 70, qMax(plot.top(), lastY - 26), 66, 18),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 %2").arg(lastValue, 0, 'f', 2).arg(m_unit));
    painter.restore();
}
