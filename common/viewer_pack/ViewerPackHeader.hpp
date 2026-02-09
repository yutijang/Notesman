#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
    #define PACKED_STRICT __pragma(pack(push, 1))
    #define END_PACKED_STRICT __pragma(pack(pop))
    #define ATTR_PACKED
#else
    #define PACKED_STRICT
    #define END_PACKED_STRICT
    #define ATTR_PACKED [[gnu::packed]]
#endif

#include <cstdint>

PACKED_STRICT

struct ViewerPackHeader final {
    public:
        // ---------------------------------------------------------------------
        // Identity
        // ---------------------------------------------------------------------
        char magic[4];            // 0..3   "RVPK"
        std::uint16_t version;    // 4..5   format version
        std::uint16_t headerSize; // 6..7   sizeof(ViewerPackHeader)

        // ---------------------------------------------------------------------
        // Resource identity
        // ---------------------------------------------------------------------
        std::int64_t resourceId; // 8..15  SQLite rowid (INTEGER PRIMARY KEY)

        // ---------------------------------------------------------------------
        // Metadata
        // ---------------------------------------------------------------------
        std::uint8_t themeMode;  // 16     enum ThemeMode
        std::uint8_t language;   // 17     enum Language
        std::uint16_t reserved1; // 18..19 padding / future flags

        // ---------------------------------------------------------------------
        // Integrity
        // ---------------------------------------------------------------------
        std::uint32_t crc32; // 20..23 CRC32(header[0..19])
};

END_PACKED_STRICT

static_assert(sizeof(ViewerPackHeader) == 24); // NOLINT(readability-magic-numbers)
