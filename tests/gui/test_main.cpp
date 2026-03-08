#define CATCH_CONFIG_RUNNER

#include <QApplication>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    int result = Catch::Session().run(argc, argv);

    return result;
}
