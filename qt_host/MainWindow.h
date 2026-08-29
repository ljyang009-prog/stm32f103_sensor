#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class ChartWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void toggleConnection();
    void readSerialData();
    void onSerialError(QSerialPort::SerialPortError error);
    void onChartModeChanged(int index);
    void clearAll();
    void exportCsv();
    void calibrateGas();
    void onGasTypeChanged();

private:
    enum Series
    {
        GasSeries = 0,
        TempSeries,
        HumiSeries,
        LightSeries,
        SeriesCount
    };

    void buildUi();
    void applyStyle();
    void parseLine(const QString &line);
    void appendLog(const QString &line);
    void addSample(int seriesIndex, qint64 timeMs, double value);
    void updateGasUi();
    void updateDhtUi();
    void updateLightUi();
    void updateChart();
    void updateAlarmBanner();
    void setPill(QLabel *label, const QString &text, const QString &background);
    double calculateMq2Ppm(double vout, double cleanAirVoltage, int gasIndex) const;

    QSerialPort *m_serial = nullptr;
    QByteArray m_rxBuffer;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_chartModeCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QCheckBox *m_timestampCheck = nullptr;
    QLabel *m_alarmBanner = nullptr;
    QLabel *m_gasValueLabel = nullptr;
    QLabel *m_gasRawLabel = nullptr;
    QLabel *m_gasStatusLabel = nullptr;
    QLabel *m_gasPpmLabel = nullptr;
    QLabel *m_gasCalibrationLabel = nullptr;
    QComboBox *m_gasTypeCombo = nullptr;
    QPushButton *m_calibrateButton = nullptr;
    QProgressBar *m_gasBar = nullptr;
    QLabel *m_tempValueLabel = nullptr;
    QLabel *m_humiValueLabel = nullptr;
    QLabel *m_tempAlarmLabel = nullptr;
    QLabel *m_humiAlarmLabel = nullptr;
    QProgressBar *m_tempBar = nullptr;
    QProgressBar *m_humiBar = nullptr;
    QLabel *m_lightValueLabel = nullptr;
    QLabel *m_lightRawLabel = nullptr;
    QLabel *m_lightStatusLabel = nullptr;
    QProgressBar *m_lightBar = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    ChartWidget *m_chart = nullptr;

    bool m_gasValid = false;
    bool m_gasLeak = false;
    double m_gasVoltage = 0.0;
    bool m_gasPpmValid = false;
    double m_gasPpm = 0.0;
    bool m_gasCalibrated = false;
    double m_gasCleanAirVoltage = 0.0;

    bool m_dhtValid = false;
    int m_temp = 0;
    int m_humi = 0;
    bool m_tempAlarm = false;
    bool m_humiAlarm = false;

    bool m_lightValid = false;
    int m_lightRaw = 0;
    int m_lightLevel = 0;
    bool m_lightDark = false;

    QVector<qint64> m_times[SeriesCount];
    QVector<double> m_values[SeriesCount];
};

#endif
