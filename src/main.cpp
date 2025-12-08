#include <QApplication>
#include "AppInitializer.hpp"
#include <QFile>
#include <QTextStream>

static QFile gLogFile;

int main(int argc, char* argv[]) {
    gLogFile.setFileName("/tmp/notesman-update-debug.log");
    gLogFile.open(QIODevice::WriteOnly | QIODevice::Append);

    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
        QTextStream ts(&gLogFile);
        ts << msg << "\n";
        ts.flush();
    });

    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");

    AppInitializer initializer;

    if (!initializer.ensureSingleInstance()) {
        return 0; // instance thứ hai thoát ngay
    }

    initializer.run();

    return app.exec(); // NOLINT(readability-static-accessed-through-instance)
}
