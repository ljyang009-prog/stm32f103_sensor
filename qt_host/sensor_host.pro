QT += core gui serialport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = sensor_host
TEMPLATE = app
CONFIG += c++17
CONFIG -= app_bundle

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    ChartWidget.cpp

HEADERS += \
    MainWindow.h \
    ChartWidget.h
