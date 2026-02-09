#include <catch2/catch_test_macros.hpp>

#include "AppSettings.hpp"
#include "UiConstants.hpp"

TEST_CASE("AppSettings - default values", "[AppSettings]") {
    AppSettings settings;

    SECTION("default language should be English") {
        REQUIRE(settings.language() == UiConst::Language::English);
    }

    SECTION("default theme should be Light") {
        REQUIRE(settings.theme() == UiConst::Theme::Light);
    }
}

TEST_CASE("AppSettings - change settings", "[AppSettings]") {
    AppSettings settings;

    settings.setLanguage(UiConst::Language::Vietnamese);
    settings.setTheme(UiConst::Theme::Dark);

    REQUIRE(settings.language() == UiConst::Language::Vietnamese);
    REQUIRE(settings.theme() == UiConst::Theme::Dark);
}
