// 图形版入口：起 Qt 应用，其余全交给 MainWindow
#include <QApplication>
#include <QString>

#include "Core/System.h"
#include "UI/MainWindow.h"
#include "UI/Style.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DataBackup"));
    app.setStyleSheet(Style::Sheet());
    System system;

    MainWindow window(system);
    window.show();

    return app.exec();
}
