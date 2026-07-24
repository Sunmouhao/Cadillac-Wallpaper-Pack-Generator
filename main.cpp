#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("CT5 Wallpaper Packager");
    QApplication::setOrganizationName("Cadillac");

    MainWindow window;
    window.show();

    return app.exec();
}
