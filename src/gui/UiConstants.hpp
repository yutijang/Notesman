#pragma once

#include <cstdint>

namespace UiConst {
    inline constexpr int FONT_SIZE{11};
    inline constexpr int BUTTON_WIDTH{120};
    inline constexpr int NOTI_TIMEOUT{3000};
    inline constexpr int BUTTON_NEXT_INPUT_WIDTH{40};
    // NOLINTNEXTLINE(readability-identifier-naming)
    enum class SettingsMessageState : std::uint8_t { None, Updated, Default };
} // namespace UiConst
