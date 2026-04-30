#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
#define PACKED_STRUCT_BEGIN __pragma(pack(push, 1))
#define PACKED_STRUCT_END __pragma(pack(pop))
#define PACKED_ATTR
#else
#define PACKED_STRUCT_BEGIN
#define PACKED_STRUCT_END
#define PACKED_ATTR __attribute__((packed))
#endif

PACKED_STRUCT_BEGIN

struct ViewerPackHeader final {
    public:
        static constexpr char RVPK_MAGIC[4] = {'R', 'V', 'P', 'K'};
        static constexpr std::uint16_t VERSION{2};
        static constexpr std::size_t UUID_LENGTH{32};

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
        char uuid[UUID_LENGTH];  // 16..47 hex string + null

        // ---------------------------------------------------------------------
        // Metadata
        // ---------------------------------------------------------------------
        std::uint8_t themeMode;  // 48     enum ThemeMode
        std::uint8_t language;   // 49     enum Language
        std::uint16_t reserved1; // 50..51 padding / future flags

        // ---------------------------------------------------------------------
        // Integrity
        // ---------------------------------------------------------------------
        std::uint32_t crc32; // 52..55 CRC32(header[0..51])
} PACKED_ATTR;

PACKED_STRUCT_END

// NOLINTBEGIN (readability-magic-numbers)
static_assert(sizeof(ViewerPackHeader) == 56);
static_assert(offsetof(ViewerPackHeader, resourceId) == 8);
static_assert(offsetof(ViewerPackHeader, uuid) == 16);
static_assert(offsetof(ViewerPackHeader, crc32) == 52);
// NOLINTEND