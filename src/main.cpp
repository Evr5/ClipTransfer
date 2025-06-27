#include <QApplication>
#include <QStyleFactory>
#include "ClipTransfer/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 36));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(24, 24, 28));
    palette.setColor(QPalette::AlternateBase, QColor(36, 36, 42));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(45, 45, 55));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor(80, 80, 160));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    MainWindow w;
    w.setWindowTitle("ClipTransfer");
    w.resize(520, 420);
    w.show();
    return app.exec();
}