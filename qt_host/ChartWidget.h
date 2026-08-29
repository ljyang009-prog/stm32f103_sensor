#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QColor>
#include <QVector>
#include <QWidget>

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);

    void setSeries(const QVector<qint64> &times, const QVector<double> &values);
    void setRange(double minY, double maxY);
    void setThreshold(double thresholdY);
    void clearThreshold();
    void setMetric(const QString &title, const QString &unit, const QColor &color);
    void clearSeries();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<qint64> m_times;
    QVector<double> m_values;
    double m_minY = 0.0;
    double m_maxY = 100.0;
    bool m_hasThreshold = false;
    double m_thresholdY = 0.0;
    QString m_title;
    QString m_unit;
    QColor m_color = QColor(0, 180, 216);
    int m_windowSeconds = 120;
};

#endif
