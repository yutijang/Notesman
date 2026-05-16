#pragma once

namespace Win32Utils {

template<typename Fn>
[[nodiscard]]
Fn loadFunction(HMODULE const module, char const* const name) noexcept {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"

    return reinterpret_cast<Fn>(GetProcAddress(module, name));

#pragma clang diagnostic pop
}

} // namespace Win32Utils
