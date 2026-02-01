#pragma once

#include <cstdint>

namespace UiConst {
    inline constexpr int FONT_SIZE{11};
    inline constexpr int BUTTON_WIDTH{120};
    inline constexpr int NOTI_TIMEOUT{3'000};
    inline constexpr int NOTI_TIMEOUT5{5'000};
    inline constexpr int BUTTON_NEXT_INPUT_WIDTH{40};
    enum class SettingsMessageState : std::uint8_t { none, updated, notChange };
    enum class SettingsTabNotiLevel : std::uint8_t { good, normal, caution, warning };
    enum class Theme : std::uint8_t { light, dark };
    enum class Language : std::uint8_t { english, vietnamese };
    enum class AddResMode : std::uint8_t { text, file, url };
} // namespace UiConst
