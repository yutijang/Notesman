#include <QApplication>
#include "AppInitializer.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    AppInitializer initializer;
    initializer.run();

    return app.exec(); // NOLINT(readability-static-accessed-through-instance)
}
