// FreeTable3D : point d'entrée.

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>
#include <QtGlobal>

#include "Icones.h"
#include "Traduction.h"
#include "MainWindow.h"

namespace
{

// Thème sombre cohérent avec la vue 3D ; l'orange est la couleur de sélection du logiciel.
void appliquerTheme(QApplication& application)
{
    application.setStyle(QStyleFactory::create("Fusion"));

    const QColor fond(0x23, 0x27, 0x2e);
    const QColor fondClair(0x2c, 0x32, 0x3b);
    const QColor champ(0x1b, 0x1f, 0x25);
    const QColor texte(0xe3, 0xe8, 0xef);
    const QColor accent(0xff, 0x8a, 0x3d);

    QPalette palette;
    palette.setColor(QPalette::Window, fond);
    palette.setColor(QPalette::WindowText, texte);
    palette.setColor(QPalette::Base, champ);
    palette.setColor(QPalette::AlternateBase, fondClair);
    palette.setColor(QPalette::ToolTipBase, fondClair);
    palette.setColor(QPalette::ToolTipText, texte);
    palette.setColor(QPalette::Text, texte);
    palette.setColor(QPalette::Button, fondClair);
    palette.setColor(QPalette::ButtonText, texte);
    palette.setColor(QPalette::BrightText, QColor(0xff, 0x5a, 0x5a));
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, QColor(0x14, 0x16, 0x1a));
    palette.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x93, 0x9f));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6c, 0x74, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6c, 0x74, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6c, 0x74, 0x80));
    application.setPalette(palette);

    application.setStyleSheet(R"(
        QToolBar { background: #2c323b; border: none; border-bottom: 1px solid #14171b; spacing: 3px; padding: 4px; }
        QToolBar::separator { background: #444c58; width: 1px; margin: 4px 6px; }
        QToolButton { border: 1px solid transparent; border-radius: 6px; padding: 4px; }
        QToolButton:hover { background: #3a424e; border-color: #4f5967; }
        QToolButton:checked { background: #4a3727; border-color: #ff8a3d; }
        QToolButton:pressed { background: #55402d; }
        QToolButton:disabled { opacity: 0.4; }
        QMenuBar { background: #23272e; }
        QMenuBar::item:selected { background: #3a424e; border-radius: 4px; }
        QMenu { background: #2c323b; border: 1px solid #14171b; padding: 4px; }
        QMenu::item { padding: 5px 22px; border-radius: 4px; }
        QMenu::item:selected { background: #ff8a3d; color: #14161a; }
        QDockWidget { font-weight: bold; }
        QDockWidget::title { background: #2c323b; padding: 6px; border-bottom: 1px solid #14171b; }
        QGroupBox { border: 1px solid #3b434f; border-radius: 8px; margin-top: 14px; padding: 8px; font-weight: bold; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #ff8a3d; }
        QGroupBox QLabel, QGroupBox QCheckBox { font-weight: normal; }
        QPushButton { background: #333a44; border: 1px solid #4a5360; border-radius: 6px; padding: 5px 10px; }
        QPushButton:hover { background: #3f4753; border-color: #ff8a3d; }
        QPushButton:pressed { background: #4a3727; }
        QPushButton:disabled { color: #6c7480; background: #2a2f37; border-color: #373e48; }
        QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit { background: #1b1f25; border: 1px solid #3b434f; border-radius: 5px; padding: 3px 6px; }
        QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QLineEdit:focus { border-color: #ff8a3d; }
        QDoubleSpinBox:disabled { color: #6c7480; background: #22262d; }
        QListWidget { background: #1b1f25; border: none; padding: 4px; }
        QListWidget::item { padding: 5px 8px; border-radius: 4px; }
        QListWidget::item:selected { background: #ff8a3d; color: #14161a; }
        QListWidget::item:hover:!selected { background: #2f3640; }
        QStatusBar { background: #1b1f25; color: #aab3bf; }
        QToolTip { background: #2c323b; color: #e3e8ef; border: 1px solid #ff8a3d; padding: 4px; }
        QScrollBar:vertical { background: #23272e; width: 10px; }
        QScrollBar::handle:vertical { background: #444c58; border-radius: 5px; min-height: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    )");
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    // Xw_Window d'OpenCascade a besoin d'une fenêtre X11, y compris sous Wayland.
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication application(argc, argv);
    application.setApplicationName("FreeTable3D");
    application.setOrganizationName("FreeTable3D");
    application.setDesktopFileName("freetable3d");
    application.setWindowIcon(icones::creer("cube"));
    traduction::installer(application);
    appliquerTheme(application);

    MainWindow fenetre;
    fenetre.show();

    return application.exec();
}
