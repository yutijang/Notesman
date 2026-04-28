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

        static constexpr std::size_t CREATED_AT_LENGTH = 19;
        static constexpr std::size_t CREATED_AT_BUFFER_SIZE = CREATED_AT_LENGTH + 1;

        // ---------------------------------------------------------------------
        // Identity
        // ---------------------------------------------------------------------
        char magic[4];            // 0..3   "RVPK"
        std::uint16_t version;    // 4..5   format version
        std::uint16_t headerSize; // 6..7   sizeof(ViewerPackHeader)

        // ---------------------------------------------------------------------
        // Resource identity
        // ---------------------------------------------------------------------
        std::int64_t resourceId;                // 8..15  SQLite rowid (INTEGER PRIMARY KEY)
        char createdAt[CREATED_AT_BUFFER_SIZE]; // 16..35  "2026-01-01 00:00:00" + null

        // ---------------------------------------------------------------------
        // Metadata
        // ---------------------------------------------------------------------
        std::uint8_t themeMode;  // 36     enum ThemeMode
        std::uint8_t language;   // 37     enum Language
        std::uint16_t reserved1; // 38..39 padding / future flags

        // ---------------------------------------------------------------------
        // Integrity
        // ---------------------------------------------------------------------
        std::uint32_t crc32; // 40..43 CRC32(header[0..39])
} PACKED_ATTR;

PACKED_STRUCT_END

// NOLINTBEGIN (readability-magic-numbers)
static_assert(sizeof(ViewerPackHeader) == 44);
static_assert(offsetof(ViewerPackHeader, resourceId) == 8);
static_assert(offsetof(ViewerPackHeader, createdAt) == 16);
static_assert(offsetof(ViewerPackHeader, crc32) == 40);
// NOLINTEND