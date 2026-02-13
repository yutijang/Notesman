#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    int result = Catch::Session().run(argc, argv);

    return result;
}
