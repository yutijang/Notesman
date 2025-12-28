#include <QApplication>

#include "Logger.hpp"
#include "AppInitializer.hpp"

#ifdef Q_OS_WIN
    #include "SecurityUtils.hpp"
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    if (!security_utils::verifyLauncherToken()) {
        Log::err("The application was run directly without using the launcher.");
        return 1;
    }

    // filter token
    // NOLINTBEGIN
    int cleanArgc = 0;
    char* cleanArgv[64];
    cleanArgv[cleanArgc++] = argv[0];
    for (int i = 2; i < argc && cleanArgc < 64; ++i) { cleanArgv[cleanArgc++] = argv[i]; }
    // NOLINTEND
#endif

    QApplication app(cleanArgc, cleanArgv);

    QApplication::setStyle("Fusion");

    Log::init();

    AppInitializer initializer;

    if (!initializer.ensureSingleInstance()) {
        Log::info("Second instance detected, exiting.");
        return 0; // instance thứ hai thoát ngay
    }

    initializer.run();

    return app.exec(); // NOLINT(readability-static-accessed-through-instance)
}
