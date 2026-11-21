// CaoLibre : point d'entrée.

#include <QApplication>
#include <QtGlobal>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    // Xw_Window d'OpenCascade a besoin d'une fenêtre X11, y compris sous Wayland.
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication application(argc, argv);

    MainWindow fenetre;
    fenetre.show();

    return application.exec();
}
