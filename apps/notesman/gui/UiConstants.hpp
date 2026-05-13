#pragma once

#include <cstdint>

namespace UiConst {

inline constexpr int FONT_SIZE{11};
inline constexpr int BUTTON_WIDTH{120};
inline constexpr int NOTI_TIMEOUT{3'000};
inline constexpr int NOTI_TIMEOUT5{5'000};
inline constexpr int BUTTON_NEXT_INPUT_WIDTH{40};
enum class SettingsMessageState : std::uint8_t { None, Updated, NotChange };
enum class SettingsTabNotiLevel : std::uint8_t { Good, Normal, Caution, Warning };
enum class Theme : std::uint8_t { Light, Dark };
enum class Language : std::uint8_t { English, Vietnamese };
enum class ResManKind : std::uint8_t { Internal, SavePathOnly };
enum class AddResMode : std::uint8_t { Text, File, Url };
enum class CleanupResult : std::uint8_t { PathError, AlreadyEmpty, Success };
enum class CleanupMode : std::uint8_t { Epub, Markdown };
enum class StatusState : std::uint8_t { Ready, Busy, Error };

} // namespace UiConst
