#include <qapplication.h>

#include "Logger.hpp"
#include "AppInitializer.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
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
