#include <QApplication>
#include "AppInitializer.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");

    AppInitializer initializer;

    if (!initializer.ensureSingleInstance()) {
        return 0; // instance thứ hai thoát ngay
    }

    initializer.run();

    return app.exec(); // NOLINT(readability-static-accessed-through-instance)
}
