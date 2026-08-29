#include "MainWindow.h"

#include "ChartWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

#include <cmath>

namespace
{
constexpr int kMaxSamples = 2400;
constexpr double kMq2Vcc = 3.3;
constexpr double kMq2LoadKOhm = 10.0;
constexpr double kMq2CleanAirRatio = 9.83;

struct Mq2Curve
{
    QString name;
    double x0;
    double y0;
    double slope;
};

const Mq2Curve kMq2Curves[] = {
    {QStringLiteral("LPG"), 2.3, 0.21, -0.47},
    {QStringLiteral("CO"), 2.3, 0.72, -0.34},
    {QStringLiteral("Smoke"), 2.3, 0.53, -0.44},
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_serial = new QSerialPort(this);

    buildUi();
    applyStyle();
    refreshPorts();

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearAll);
    connect(m_exportButton, &QPushButton::clicked, this, &MainWindow::exportCsv);
    connect(m_calibrateButton, &QPushButton::clicked, this, &MainWindow::calibrateGas);
    connect(m_gasTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onGasTypeChanged);
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_chartModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onChartModeChanged);

    updateChart();
    statusBar()->showMessage(QStringLiteral("Ready. Select a serial port and press Open."));
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("STM32 Sensor Serial Host"));
    resize(1160, 760);
    setMinimumSize(960, 620);

    QWidget *central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    m_alarmBanner = new QLabel(central);
    m_alarmBanner->setObjectName(QStringLiteral("alarmBanner"));
    m_alarmBanner->setAlignment(Qt::AlignCenter);
    m_alarmBanner->setVisible(false);
    rootLayout->addWidget(m_alarmBanner);

    QHBoxLayout *connectionRow = new QHBoxLayout;
    connectionRow->addWidget(new QLabel(QStringLiteral("Port:"), central));
    m_portCombo = new QComboBox(central);
    m_portCombo->setMinimumWidth(220);
    connectionRow->addWidget(m_portCombo);

    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), central);
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connectionRow->addWidget(m_refreshButton);

    connectionRow->addSpacing(12);
    connectionRow->addWidget(new QLabel(QStringLiteral("Baud:"), central));
    m_baudCombo = new QComboBox(central);
    m_baudCombo->addItems({QStringLiteral("9600"), QStringLiteral("19200"),
                           QStringLiteral("38400"), QStringLiteral("57600"),
                           QStringLiteral("115200"), QStringLiteral("230400"),
                           QStringLiteral("460800"), QStringLiteral("921600")});
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    connectionRow->addWidget(m_baudCombo);

    m_connectButton = new QPushButton(QStringLiteral("Open"), central);
    m_connectButton->setObjectName(QStringLiteral("primaryButton"));
    connectionRow->addWidget(m_connectButton);
    connectionRow->addStretch(1);

    m_clearButton = new QPushButton(QStringLiteral("Clear"), central);
    connectionRow->addWidget(m_clearButton);
    m_exportButton = new QPushButton(QStringLiteral("Export CSV"), central);
    connectionRow->addWidget(m_exportButton);
    rootLayout->addLayout(connectionRow);

    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(10);

    QWidget *leftPanel = new QWidget(central);
    leftPanel->setFixedWidth(360);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    QGroupBox *gasBox = new QGroupBox(QStringLiteral("Gas / Smoke  (MQ-2)"), leftPanel);
    QVBoxLayout *gasLayout = new QVBoxLayout(gasBox);
    QHBoxLayout *gasHeader = new QHBoxLayout;
    m_gasValueLabel = new QLabel(QStringLiteral("-- V"), gasBox);
    m_gasValueLabel->setObjectName(QStringLiteral("valueLabel"));
    m_gasStatusLabel = new QLabel(QStringLiteral("WAIT"), gasBox);
    gasHeader->addWidget(m_gasValueLabel);
    gasHeader->addStretch(1);
    gasHeader->addWidget(m_gasStatusLabel);
    gasLayout->addLayout(gasHeader);
    m_gasRawLabel = new QLabel(QStringLiteral("Range: 0.00 - 3.30 V"), gasBox);
    m_gasRawLabel->setObjectName(QStringLiteral("subLabel"));
    gasLayout->addWidget(m_gasRawLabel);
    m_gasBar = new QProgressBar(gasBox);
    m_gasBar->setRange(0, 100);
    m_gasBar->setValue(0);
    m_gasBar->setFormat(QStringLiteral("-- V"));
    gasLayout->addWidget(m_gasBar);
    m_gasPpmLabel = new QLabel(QStringLiteral("Estimated PPM: --"), gasBox);
    m_gasPpmLabel->setObjectName(QStringLiteral("subLabel"));
    gasLayout->addWidget(m_gasPpmLabel);
    m_gasCalibrationLabel = new QLabel(QStringLiteral("Not calibrated"), gasBox);
    m_gasCalibrationLabel->setObjectName(QStringLiteral("subLabel"));
    gasLayout->addWidget(m_gasCalibrationLabel);

    QHBoxLayout *gasCalibrationRow = new QHBoxLayout;
    gasCalibrationRow->addWidget(new QLabel(QStringLiteral("Gas:"), gasBox));
    m_gasTypeCombo = new QComboBox(gasBox);
    m_gasTypeCombo->addItems({QStringLiteral("LPG"),
                              QStringLiteral("CO"),
                              QStringLiteral("Smoke")});
    gasCalibrationRow->addWidget(m_gasTypeCombo, 1);
    m_calibrateButton = new QPushButton(QStringLiteral("Calibrate clean air"), gasBox);
    gasCalibrationRow->addWidget(m_calibrateButton);
    gasLayout->addLayout(gasCalibrationRow);
    leftLayout->addWidget(gasBox);

    QGroupBox *dhtBox = new QGroupBox(QStringLiteral("Temperature & Humidity  (DHT11)"), leftPanel);
    QHBoxLayout *dhtLayout = new QHBoxLayout(dhtBox);

    QVBoxLayout *tempLayout = new QVBoxLayout;
    tempLayout->addWidget(new QLabel(QStringLiteral("Temperature"), dhtBox));
    m_tempValueLabel = new QLabel(QStringLiteral("-- C"), dhtBox);
    m_tempValueLabel->setObjectName(QStringLiteral("valueLabel"));
    m_tempValueLabel->setStyleSheet(QStringLiteral("color: #f4f8fb; font-size: 22px; font-weight: 700;"));
    tempLayout->addWidget(m_tempValueLabel);
    m_tempAlarmLabel = new QLabel(QStringLiteral("WAIT"), dhtBox);
    tempLayout->addWidget(m_tempAlarmLabel);
    m_tempBar = new QProgressBar(dhtBox);
    m_tempBar->setRange(0, 50);
    m_tempBar->setValue(0);
    tempLayout->addWidget(m_tempBar);
    dhtLayout->addLayout(tempLayout, 1);

    QVBoxLayout *humiLayout = new QVBoxLayout;
    humiLayout->addWidget(new QLabel(QStringLiteral("Humidity"), dhtBox));
    m_humiValueLabel = new QLabel(QStringLiteral("-- %"), dhtBox);
    m_humiValueLabel->setObjectName(QStringLiteral("valueLabel"));
    m_humiValueLabel->setStyleSheet(QStringLiteral("color: #f4f8fb; font-size: 22px; font-weight: 700;"));
    humiLayout->addWidget(m_humiValueLabel);
    m_humiAlarmLabel = new QLabel(QStringLiteral("WAIT"), dhtBox);
    humiLayout->addWidget(m_humiAlarmLabel);
    m_humiBar = new QProgressBar(dhtBox);
    m_humiBar->setRange(0, 100);
    m_humiBar->setValue(0);
    humiLayout->addWidget(m_humiBar);
    dhtLayout->addLayout(humiLayout, 1);
    leftLayout->addWidget(dhtBox);

    QGroupBox *lightBox = new QGroupBox(QStringLiteral("Light  (photoresistor)"), leftPanel);
    QVBoxLayout *lightLayout = new QVBoxLayout(lightBox);
    QHBoxLayout *lightHeader = new QHBoxLayout;
    m_lightValueLabel = new QLabel(QStringLiteral("-- %"), lightBox);
    m_lightValueLabel->setObjectName(QStringLiteral("valueLabel"));
    m_lightStatusLabel = new QLabel(QStringLiteral("WAIT"), lightBox);
    lightHeader->addWidget(m_lightValueLabel);
    lightHeader->addStretch(1);
    lightHeader->addWidget(m_lightStatusLabel);
    lightLayout->addLayout(lightHeader);
    m_lightRawLabel = new QLabel(QStringLiteral("Raw ADC: --"), lightBox);
    m_lightRawLabel->setObjectName(QStringLiteral("subLabel"));
    lightLayout->addWidget(m_lightRawLabel);
    m_lightBar = new QProgressBar(lightBox);
    m_lightBar->setRange(0, 100);
    m_lightBar->setValue(0);
    lightLayout->addWidget(m_lightBar);
    leftLayout->addWidget(lightBox);
    leftLayout->addStretch(1);

    QWidget *rightPanel = new QWidget(central);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    QGroupBox *chartBox = new QGroupBox(QStringLiteral("History"), rightPanel);
    QVBoxLayout *chartLayout = new QVBoxLayout(chartBox);
    QHBoxLayout *chartHeader = new QHBoxLayout;
    chartHeader->addWidget(new QLabel(QStringLiteral("Metric:"), chartBox));
    m_chartModeCombo = new QComboBox(chartBox);
    m_chartModeCombo->addItems({QStringLiteral("Gas voltage (V)"),
                                QStringLiteral("Temperature (C)"),
                                QStringLiteral("Humidity (%)"),
                                QStringLiteral("Light level (%)")});
    chartHeader->addWidget(m_chartModeCombo);
    chartHeader->addStretch(1);
    chartLayout->addLayout(chartHeader);
    m_chart = new ChartWidget(chartBox);
    chartLayout->addWidget(m_chart, 1);
    rightLayout->addWidget(chartBox, 3);

    QGroupBox *logBox = new QGroupBox(QStringLiteral("Raw serial log"), rightPanel);
    QVBoxLayout *logLayout = new QVBoxLayout(logBox);
    m_timestampCheck = new QCheckBox(QStringLiteral("Timestamp"), logBox);
    m_timestampCheck->setChecked(true);
    logLayout->addWidget(m_timestampCheck);
    m_logEdit = new QPlainTextEdit(logBox);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(2000);
    m_logEdit->setPlaceholderText(QStringLiteral("Serial data will appear here."));
    logLayout->addWidget(m_logEdit, 1);
    rightLayout->addWidget(logBox, 2);

    contentLayout->addWidget(leftPanel);
    contentLayout->addWidget(rightPanel, 1);
    rootLayout->addLayout(contentLayout, 1);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Ready."));
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget {
            background-color: #0d1b2a;
            color: #dbe7ef;
            font-size: 13px;
        }
        QGroupBox {
            background-color: #142433;
            border: 1px solid #274257;
            border-radius: 6px;
            margin-top: 14px;
            padding-top: 8px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #7fd1e0;
        }
        QPushButton {
            background-color: #1d3a4e;
            border: 1px solid #2d5068;
            border-radius: 4px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #24506a;
        }
        QPushButton:pressed {
            background-color: #173246;
        }
        QPushButton:disabled {
            color: #66717a;
            background-color: #14202b;
        }
        QPushButton#primaryButton {
            background-color: #0089a7;
            border-color: #00b4d8;
            color: white;
            font-weight: 700;
        }
        QPushButton#primaryButton:hover {
            background-color: #00a0c4;
        }
        QComboBox {
            background-color: #0f1e2b;
            border: 1px solid #2d5068;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QComboBox QAbstractItemView {
            background-color: #10212f;
            border: 1px solid #2d5068;
            selection-background-color: #24506a;
        }
        QPlainTextEdit {
            background-color: #08131d;
            border: 1px solid #2d5068;
            border-radius: 4px;
            font-family: "DejaVu Sans Mono", "Consolas", monospace;
            font-size: 12px;
        }
        QProgressBar {
            background-color: #0a151f;
            border: 1px solid #2d5068;
            border-radius: 4px;
            text-align: center;
            height: 18px;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background-color: #00b4d8;
        }
        QCheckBox {
            spacing: 6px;
        }
        QStatusBar {
            background-color: #0d1b2a;
            color: #aac3d0;
        }
        QLabel#valueLabel {
            color: #f4f8fb;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel#subLabel {
            color: #8aa5b2;
            font-size: 12px;
        }
        QLabel#alarmBanner {
            background-color: #c1121f;
            color: #ffffff;
            font-size: 15px;
            font-weight: 700;
            padding: 6px;
            border-radius: 4px;
        }
    )"));

    m_gasBar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background-color: #ffb703; }"));
    m_tempBar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background-color: #ef476f; }"));
    m_humiBar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background-color: #00b4d8; }"));
    m_lightBar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background-color: #90be6d; }"));
}

void MainWindow::refreshPorts()
{
    const QString previousPort = m_portCombo->currentData().toString();
    m_portCombo->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
    {
        QString label = info.portName();
        if (!info.description().isEmpty())
            label += QStringLiteral("  (%1)").arg(info.description());
        m_portCombo->addItem(label, info.portName());
    }

    if (m_portCombo->count() == 0)
    {
        m_portCombo->addItem(QStringLiteral("No serial port found"), QString());
        statusBar()->showMessage(QStringLiteral("No serial port found. Check the USB adapter or VM port mapping."));
    }
    else
    {
        const int index = m_portCombo->findData(previousPort);
        m_portCombo->setCurrentIndex(index >= 0 ? index : 0);
        statusBar()->showMessage(QStringLiteral("Found %1 serial port(s).").arg(ports.size()));
    }
}

void MainWindow::toggleConnection()
{
    if (m_serial->isOpen())
    {
        m_serial->close();
        m_connectButton->setText(QStringLiteral("Open"));
        m_connectButton->setStyleSheet(QString());
        statusBar()->showMessage(QStringLiteral("Serial port closed."));
        return;
    }

    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Serial port"),
                             QStringLiteral("No serial port is available. Refresh the port list or check the VM USB mapping."));
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_baudCombo->currentText().toInt());
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite))
    {
        QMessageBox::warning(this, QStringLiteral("Open failed"),
                             QStringLiteral("Cannot open %1:\n%2").arg(portName, m_serial->errorString()));
        return;
    }

    m_connectButton->setText(QStringLiteral("Close"));
    m_connectButton->setStyleSheet(QStringLiteral("QPushButton { background-color: #a93f4a; border-color: #d1666f; color: white; }"));
    statusBar()->showMessage(QStringLiteral("Opened %1 at %2 baud, 8N1.").arg(portName, m_baudCombo->currentText()));
}

void MainWindow::readSerialData()
{
    if (!m_serial)
        return;

    m_rxBuffer.append(m_serial->readAll());
    int newlineIndex = -1;
    while ((newlineIndex = m_rxBuffer.indexOf('\n')) >= 0)
    {
        const QByteArray lineBytes = m_rxBuffer.left(newlineIndex);
        m_rxBuffer.remove(0, newlineIndex + 1);
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (!line.isEmpty())
            parseLine(line);
    }

    if (m_rxBuffer.size() > 8192)
        m_rxBuffer.clear();
}

void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    statusBar()->showMessage(QStringLiteral("Serial error: %1").arg(m_serial->errorString()), 10000);
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError)
    {
        m_serial->close();
        m_connectButton->setText(QStringLiteral("Open"));
        m_connectButton->setStyleSheet(QString());
    }
}

void MainWindow::onChartModeChanged(int index)
{
    Q_UNUSED(index)
    updateChart();
}

void MainWindow::clearAll()
{
    for (int i = 0; i < SeriesCount; ++i)
    {
        m_times[i].clear();
        m_values[i].clear();
    }
    m_rxBuffer.clear();
    m_logEdit->clear();
    m_chart->clearSeries();

    m_gasValid = false;
    m_gasLeak = false;
    m_gasPpmValid = false;
    m_gasPpm = 0.0;
    m_dhtValid = false;
    m_lightValid = false;

    m_gasValueLabel->setText(QStringLiteral("-- V"));
    m_gasRawLabel->setText(QStringLiteral("Range: 0.00 - 3.30 V"));
    m_gasBar->setValue(0);
    m_gasBar->setFormat(QStringLiteral("-- V"));
    m_gasPpmLabel->setText(QStringLiteral("Estimated PPM: --"));
    setPill(m_gasStatusLabel, QStringLiteral("WAIT"), QStringLiteral("#44566a"));

    m_tempValueLabel->setText(QStringLiteral("-- C"));
    m_humiValueLabel->setText(QStringLiteral("-- %"));
    m_tempBar->setValue(0);
    m_humiBar->setValue(0);
    setPill(m_tempAlarmLabel, QStringLiteral("WAIT"), QStringLiteral("#44566a"));
    setPill(m_humiAlarmLabel, QStringLiteral("WAIT"), QStringLiteral("#44566a"));

    m_lightValueLabel->setText(QStringLiteral("-- %"));
    m_lightRawLabel->setText(QStringLiteral("Raw ADC: --"));
    m_lightBar->setValue(0);
    setPill(m_lightStatusLabel, QStringLiteral("WAIT"), QStringLiteral("#44566a"));

    updateAlarmBanner();
    statusBar()->showMessage(QStringLiteral("Data and log cleared."));
}

void MainWindow::calibrateGas()
{
    if (!m_gasValid)
    {
        statusBar()->showMessage(QStringLiteral("Wait for MQ-2 data before calibrating."));
        return;
    }
    if (m_gasPpmValid)
    {
        statusBar()->showMessage(
            QStringLiteral("Current firmware already provides live ppm; voltage calibration is unavailable."));
        return;
    }

    m_gasCleanAirVoltage = m_gasVoltage;
    m_gasCalibrated = true;
    updateGasUi();
    statusBar()->showMessage(QStringLiteral("MQ-2 calibrated at clean-air voltage %1 V.")
                                 .arg(m_gasCleanAirVoltage, 0, 'f', 3),
                             10000);
}

void MainWindow::onGasTypeChanged()
{
    updateGasUi();
}

double MainWindow::calculateMq2Ppm(double vout, double cleanAirVoltage, int gasIndex) const
{
    if (gasIndex < 0 || gasIndex >= 3)
        return -1.0;
    if (vout <= 0.001 || vout >= kMq2Vcc)
        return -1.0;
    if (cleanAirVoltage <= 0.001 || cleanAirVoltage >= kMq2Vcc)
        return -1.0;

    const double rs = kMq2LoadKOhm * (kMq2Vcc - vout) / vout;
    const double rsCleanAir = kMq2LoadKOhm * (kMq2Vcc - cleanAirVoltage) / cleanAirVoltage;
    const double r0 = rsCleanAir / kMq2CleanAirRatio;
    const double ratio = rs / r0;
    if (ratio <= 0.0)
        return -1.0;

    const Mq2Curve &curve = kMq2Curves[gasIndex];
    const double logPpm = curve.x0 + (std::log10(ratio) - curve.y0) / curve.slope;
    return std::pow(10.0, logPpm);
}

void MainWindow::exportCsv()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export history as CSV"), QStringLiteral("sensor_history.csv"),
        QStringLiteral("CSV files (*.csv)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, QStringLiteral("Export failed"),
                             QStringLiteral("Cannot write %1:\n%2").arg(fileName, file.errorString()));
        return;
    }

    const QStringList metricNames = {QStringLiteral("gas_voltage_v"),
                                     QStringLiteral("temperature_c"),
                                     QStringLiteral("humidity_percent"),
                                     QStringLiteral("light_percent")};

    QTextStream out(&file);
    out << "time,metric,value\n";
    for (int series = 0; series < SeriesCount; ++series)
    {
        for (int i = 0; i < m_times[series].size(); ++i)
        {
            const QString time = QDateTime::fromMSecsSinceEpoch(m_times[series].at(i))
                                     .toString(Qt::ISODate);
            out << time << ',' << metricNames.at(series) << ','
                << QString::number(m_values[series].at(i), 'f', 2) << '\n';
        }
    }
    file.close();

    statusBar()->showMessage(QStringLiteral("Exported %1 CSV rows to %2")
                                 .arg(m_times[GasSeries].size() + m_times[TempSeries].size()
                                          + m_times[HumiSeries].size() + m_times[LightSeries].size())
                                 .arg(fileName),
                             10000);
}

void MainWindow::parseLine(const QString &line)
{
    appendLog(line);

    static const QRegularExpression gasPpmRe(QStringLiteral(
        "^Gas (leakage|not leakage)!!! ppm=([0-9]+(?:\\.[0-9]+)?)"));
    static const QRegularExpression gasVoltageRe(QStringLiteral(
        "^Gas (leakage|not leakage)!!! ad_value:([0-9]+(?:\\.[0-9]+)?)V"));
    static const QRegularExpression dhtRe(QStringLiteral(
        "^DHT11: temp=(-?[0-9]+)C humi=([0-9]+)% temp_alarm=([01]) humi_alarm=([01])"));
    static const QRegularExpression dhtErrorRe(QStringLiteral("^DHT11: read error"));
    static const QRegularExpression lightRe(QStringLiteral(
        "^LIGHT: raw=([0-9]+) level=([0-9]+)% dark=([01])"));

    QRegularExpressionMatch match = gasPpmRe.match(line);
    if (match.hasMatch())
    {
        m_gasValid = true;
        m_gasPpmValid = true;
        m_gasLeak = (match.captured(1) == QStringLiteral("leakage"));
        m_gasPpm = match.captured(2).toDouble();
        addSample(GasSeries, QDateTime::currentMSecsSinceEpoch(), m_gasPpm);
        updateGasUi();
        updateAlarmBanner();
        updateChart();
        return;
    }

    match = gasVoltageRe.match(line);
    if (match.hasMatch())
    {
        m_gasValid = true;
        m_gasPpmValid = false;
        m_gasLeak = (match.captured(1) == QStringLiteral("leakage"));
        m_gasVoltage = match.captured(2).toDouble();
        addSample(GasSeries, QDateTime::currentMSecsSinceEpoch(), m_gasVoltage);
        updateGasUi();
        updateAlarmBanner();
        updateChart();
        return;
    }

    match = dhtErrorRe.match(line);
    if (match.hasMatch())
    {
        m_dhtValid = false;
        updateDhtUi();
        updateAlarmBanner();
        return;
    }

    match = dhtRe.match(line);
    if (match.hasMatch())
    {
        m_dhtValid = true;
        m_temp = match.captured(1).toInt();
        m_humi = match.captured(2).toInt();
        m_tempAlarm = (match.captured(3) == QStringLiteral("1"));
        m_humiAlarm = (match.captured(4) == QStringLiteral("1"));
        addSample(TempSeries, QDateTime::currentMSecsSinceEpoch(), m_temp);
        addSample(HumiSeries, QDateTime::currentMSecsSinceEpoch(), m_humi);
        updateDhtUi();
        updateAlarmBanner();
        updateChart();
        return;
    }

    match = lightRe.match(line);
    if (match.hasMatch())
    {
        m_lightValid = true;
        m_lightRaw = match.captured(1).toInt();
        m_lightLevel = match.captured(2).toInt();
        m_lightDark = (match.captured(3) == QStringLiteral("1"));
        addSample(LightSeries, QDateTime::currentMSecsSinceEpoch(), m_lightLevel);
        updateLightUi();
        updateChart();
        return;
    }
}

void MainWindow::appendLog(const QString &line)
{
    if (m_timestampCheck->isChecked())
    {
        m_logEdit->appendPlainText(QStringLiteral("[%1] %2")
                                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                            line));
    }
    else
    {
        m_logEdit->appendPlainText(line);
    }
}

void MainWindow::addSample(int seriesIndex, qint64 timeMs, double value)
{
    m_times[seriesIndex].append(timeMs);
    m_values[seriesIndex].append(value);

    if (m_times[seriesIndex].size() > kMaxSamples)
    {
        while (m_times[seriesIndex].size() > kMaxSamples)
        {
            m_times[seriesIndex].removeFirst();
            m_values[seriesIndex].removeFirst();
        }
    }
}

void MainWindow::updateGasUi()
{
    const int gasIndex = m_gasTypeCombo->currentIndex();
    const QString gasName = kMq2Curves[gasIndex].name;
    if (m_gasPpmValid)
    {
        m_gasValueLabel->setText(QStringLiteral("%1 ppm").arg(m_gasPpm, 0, 'f', 1));
        m_gasRawLabel->setText(QStringLiteral("Source: STM32 MQ-2 estimate"));
        m_gasBar->setValue(qBound(0, qRound(m_gasPpm / 1000.0 * 100.0), 100));
        m_gasBar->setFormat(QStringLiteral("%1 ppm").arg(m_gasPpm, 0, 'f', 1));
        m_gasPpmLabel->setText(QStringLiteral("Live concentration: %1 ppm")
                                   .arg(m_gasPpm, 0, 'f', 1));
        m_gasCalibrationLabel->setText(QStringLiteral("Voltage not provided by firmware"));
    }
    else
    {
        m_gasValueLabel->setText(QStringLiteral("%1 V").arg(m_gasVoltage, 0, 'f', 2));
        m_gasRawLabel->setText(QStringLiteral("Range: 0.00 - 3.30 V"));
        m_gasBar->setValue(qBound(0, qRound(m_gasVoltage / 3.3 * 100.0), 100));
        m_gasBar->setFormat(QStringLiteral("%1 V").arg(m_gasVoltage, 0, 'f', 2));

        if (m_gasCalibrated)
        {
            const double ppm = calculateMq2Ppm(m_gasVoltage, m_gasCleanAirVoltage, gasIndex);
            if (ppm >= 0.0)
                m_gasPpmLabel->setText(QStringLiteral("Estimated %1: %2 ppm")
                                           .arg(gasName)
                                           .arg(ppm, 0, 'f', 1));
            else
                m_gasPpmLabel->setText(QStringLiteral("Estimated %1: -- ppm").arg(gasName));
            m_gasCalibrationLabel->setText(QStringLiteral("Calibrated at %1 V")
                                               .arg(m_gasCleanAirVoltage, 0, 'f', 3));
        }
        else
        {
            m_gasPpmLabel->setText(QStringLiteral("Estimated %1: -- ppm").arg(gasName));
            m_gasCalibrationLabel->setText(QStringLiteral("Not calibrated"));
        }
    }

    if (m_gasLeak)
        setPill(m_gasStatusLabel, QStringLiteral("LEAK"), QStringLiteral("#c1121f"));
    else
        setPill(m_gasStatusLabel, QStringLiteral("NORMAL"), QStringLiteral("#2a9d8f"));
}

void MainWindow::updateDhtUi()
{
    if (!m_dhtValid)
    {
        m_tempValueLabel->setText(QStringLiteral("-- C"));
        m_humiValueLabel->setText(QStringLiteral("-- %"));
        m_tempBar->setValue(0);
        m_humiBar->setValue(0);
        setPill(m_tempAlarmLabel, QStringLiteral("ERR"), QStringLiteral("#c1121f"));
        setPill(m_humiAlarmLabel, QStringLiteral("ERR"), QStringLiteral("#c1121f"));
        return;
    }

    m_tempValueLabel->setText(QStringLiteral("%1 C").arg(m_temp));
    m_humiValueLabel->setText(QStringLiteral("%1 %").arg(m_humi));
    m_tempBar->setValue(qBound(0, m_temp, 50));
    m_humiBar->setValue(qBound(0, m_humi, 100));

    if (m_tempAlarm)
        setPill(m_tempAlarmLabel, QStringLiteral("ALARM"), QStringLiteral("#c1121f"));
    else
        setPill(m_tempAlarmLabel, QStringLiteral("OK"), QStringLiteral("#2a9d8f"));

    if (m_humiAlarm)
        setPill(m_humiAlarmLabel, QStringLiteral("ALARM"), QStringLiteral("#c1121f"));
    else
        setPill(m_humiAlarmLabel, QStringLiteral("OK"), QStringLiteral("#2a9d8f"));
}

void MainWindow::updateLightUi()
{
    m_lightValueLabel->setText(QStringLiteral("%1 %").arg(m_lightLevel));
    m_lightRawLabel->setText(QStringLiteral("Raw ADC: %1 / 4096").arg(m_lightRaw));
    m_lightBar->setValue(qBound(0, m_lightLevel, 100));

    if (m_lightDark)
        setPill(m_lightStatusLabel, QStringLiteral("DARK"), QStringLiteral("#c1121f"));
    else
        setPill(m_lightStatusLabel, QStringLiteral("LIGHT"), QStringLiteral("#2a9d8f"));
}

void MainWindow::updateChart()
{
    const int mode = m_chartModeCombo->currentIndex();
    if (mode < 0 || mode >= SeriesCount)
        return;

    m_chart->clearThreshold();

    switch (mode)
    {
    case GasSeries:
        m_chart->setMetric(m_gasPpmValid ? QStringLiteral("MQ-2 gas concentration")
                                         : QStringLiteral("MQ-2 gas voltage"),
                           m_gasPpmValid ? QStringLiteral("ppm") : QStringLiteral("V"),
                           QColor(255, 183, 3));
        if (m_gasPpmValid)
            m_chart->setRange(0.0, 1000.0);
        else
        {
            m_chart->setRange(0.0, 3.3);
            m_chart->setThreshold(2.5);
        }
        break;
    case TempSeries:
        m_chart->setMetric(QStringLiteral("DHT11 temperature"), QStringLiteral("C"),
                           QColor(239, 71, 111));
        m_chart->setRange(0.0, 50.0);
        break;
    case HumiSeries:
        m_chart->setMetric(QStringLiteral("DHT11 humidity"), QStringLiteral("%"),
                           QColor(0, 180, 216));
        m_chart->setRange(0.0, 100.0);
        break;
    case LightSeries:
        m_chart->setMetric(QStringLiteral("Light level"), QStringLiteral("%"),
                           QColor(144, 190, 109));
        m_chart->setRange(0.0, 100.0);
        m_chart->setThreshold(50.0);
        break;
    }

    m_chart->setSeries(m_times[mode], m_values[mode]);
}

void MainWindow::updateAlarmBanner()
{
    QStringList alarms;
    if (m_gasValid && m_gasLeak)
        alarms << QStringLiteral("Gas leak");
    if (m_dhtValid)
    {
        if (m_tempAlarm)
            alarms << QStringLiteral("Temperature out of range");
        if (m_humiAlarm)
            alarms << QStringLiteral("Humidity out of range");
    }

    m_alarmBanner->setVisible(!alarms.isEmpty());
    if (!alarms.isEmpty())
        m_alarmBanner->setText(QStringLiteral("ALARM: %1").arg(alarms.join(QStringLiteral(" | "))));
}

void MainWindow::setPill(QLabel *label, const QString &text, const QString &background)
{
    label->setText(text);
    label->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; border-radius: 8px; padding: 2px 10px; font-weight: 700;")
                             .arg(background));
}
