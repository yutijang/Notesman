#include "common/viewer_pack/ViewerPackCrc32.hpp"
#include "common/viewer_pack/ViewerPackError.hpp"
#include "common/viewer_pack/ViewerPackHeader.hpp"
#include "common/viewer_pack/ViewerPackWriter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>

namespace fs = std::filesystem;

namespace {
ViewerPackHeader makeHeader() {
    ViewerPackHeader header{};

    std::memcpy(header.magic, ViewerPackHeader::RVPK_MAGIC, 4);
    header.version = ViewerPackHeader::VERSION;

    return header;
}

fs::path makeTempDir() {
    fs::path const dir = fs::temp_directory_path() / "ViewerPackWriterTests";
    fs::create_directory(dir);

    return dir;
}
}; // namespace

TEST_CASE("ViewerPackWriter writes file successfully", "[ViewerPackWriter]") {
    fs::path const dir = makeTempDir();
    fs::path const filePath = dir / "test.rvpk";

    ViewerPackWriter writer;
    auto header = makeHeader();

    auto result = writer.write(filePath, header);

    REQUIRE(result.has_value());
    REQUIRE(fs::exists(filePath));

    fs::remove_all(dir);
}

TEST_CASE("Written header is valid and CRC matches", "[ViewerPackWriter]") {
    fs::path const dir = makeTempDir();
    fs::path const filePath = dir / "test.rvpk";

    ViewerPackWriter writer;
    auto header = makeHeader();

    REQUIRE(writer.write(filePath, header).has_value());

    std::ifstream ifs(filePath, std::ios::binary);
    REQUIRE(ifs.is_open());

    ViewerPackHeader readHeader{};
    ifs.read(reinterpret_cast<char*>(&readHeader), sizeof(readHeader));

    REQUIRE(readHeader.headerSize == sizeof(ViewerPackHeader));
    REQUIRE(readHeader.version == ViewerPackHeader::VERSION);

    constexpr std::size_t crcOffset = offsetof(ViewerPackHeader, crc32);
    auto const expectedCrc = computeCrc32(&readHeader, crcOffset);

    REQUIRE(readHeader.crc32 == expectedCrc);

    fs::remove_all(dir);
}

TEST_CASE("Write fails when directory does not exist", "[ViewerPackWriter]") {
    fs::path const dir = fs::temp_directory_path() / "NonExistDir";
    fs::path const filePath = dir / "test.rvpk";

    ViewerPackWriter writer;
    auto header = makeHeader();

    auto result = writer.write(filePath, header);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ViewerPackError::FileOpenFailed);
}

TEST_CASE("Temporary file is removed after successful write", "[ViewerPackWriter]") {
    fs::path const dir = makeTempDir();
    fs::path const filePath = dir / "test.rvpk";
    fs::path const tempPath = filePath.string() + ".tmp";

    ViewerPackWriter writer;
    auto header = makeHeader();

    REQUIRE(writer.write(filePath, header).has_value());

    REQUIRE_FALSE(fs::exists(tempPath));

    fs::remove_all(dir);
}
