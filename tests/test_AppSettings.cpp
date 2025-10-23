#include <catch2/catch_test_macros.hpp>
#include "AppSettings.hpp"

TEST_CASE("AppSettings - default values", "[AppSettings]") {
    AppSettings settings;

    SECTION("default language should be English") {
        REQUIRE(settings.language() == Language::english);
    }

    SECTION("default theme should be Light") {
        REQUIRE(settings.theme() == Theme::light);
    }
}

TEST_CASE("AppSettings - change settings", "[AppSettings]") {
    AppSettings settings;

    settings.setLanguage(Language::vietnamese);
    settings.setTheme(Theme::dark);

    REQUIRE(settings.language() == Language::vietnamese);
    REQUIRE(settings.theme() == Theme::dark);
}
