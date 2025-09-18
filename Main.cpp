#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName("VeilbreakerSoftware");
    QCoreApplication::setApplicationName("StockHound");

    MainWindow window;

    window.setWindowTitle("StockHound");
    window.show();

    return application.exec();
}
