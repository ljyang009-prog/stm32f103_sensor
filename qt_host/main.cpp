#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("STM32 Sensor Serial Host"));
    app.setOrganizationName(QStringLiteral("STM32Sensor"));

    MainWindow window;
    window.show();
    return app.exec();
}
