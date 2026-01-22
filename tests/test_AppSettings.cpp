#include <catch2/catch_test_macros.hpp>

#include "AppSettings.hpp"
#include "UiConstants.hpp"

TEST_CASE("AppSettings - default values", "[AppSettings]") {
    AppSettings settings;

    SECTION("default language should be English") {
        REQUIRE(settings.language() == UiConst::Language::english);
    }

    SECTION("default theme should be Light") {
        REQUIRE(settings.theme() == UiConst::Theme::light);
    }
}

TEST_CASE("AppSettings - change settings", "[AppSettings]") {
    AppSettings settings;

    settings.setLanguage(UiConst::Language::vietnamese);
    settings.setTheme(UiConst::Theme::dark);

    REQUIRE(settings.language() == UiConst::Language::vietnamese);
    REQUIRE(settings.theme() == UiConst::Theme::dark);
}
