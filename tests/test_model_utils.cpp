#include <stdexcept>
#include <optional>
#include <string_view>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/catch_test_macros.hpp>
#include "model.hpp"

TEST_CASE("resourceTypeToString - Standard Mappings", "[Model][Utils][ResourceType]") {
    SECTION("Mapping text") {
        CHECK(std::string_view(resourceTypeToString(ResourceType::plainText)) == "text");
    }

    SECTION("Mapping cpp") {
        CHECK(std::string_view(resourceTypeToString(ResourceType::cCppCode)) == "cpp");
    }

    SECTION("Mapping pdf") {
        CHECK(std::string_view(resourceTypeToString(ResourceType::pdfDoc)) == "pdf");
    }

    SECTION("Mapping epub") {
        CHECK(std::string_view(resourceTypeToString(ResourceType::epubDoc)) == "epub");
    }
}

TEST_CASE("resourceTypeFromString - Valid and Invalid Inputs", "[Model][Utils][ResourceType]") {
    SECTION("Valid String Conversions") {
        REQUIRE(resourceTypeFromString("text") == ResourceType::plainText);
        REQUIRE(resourceTypeFromString("cpp") == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromString("pdf") == ResourceType::pdfDoc);
        REQUIRE(resourceTypeFromString("epub") == ResourceType::epubDoc);
    }

    SECTION("Invalid String Should Throw std::runtime_error") {
        CHECK_THROWS_AS(resourceTypeFromString("unknow_type"), std::runtime_error);
        CHECK_THROWS_AS(resourceTypeFromString("Cpp"), std::runtime_error);
        CHECK_THROWS_AS(resourceTypeFromString(""), std::runtime_error);
        CHECK_THROWS_WITH(resourceTypeFromString("unknow"), "Unknown ResourceType string: unknow");
    }
}

TEST_CASE("resourceTypeFromExtension", "[Model][Utils][ResourceType]") {
    SECTION("Mapping valid resource type") {
        REQUIRE(resourceTypeFromExtension("txt") == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromExtension("cpp") == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromExtension("h") == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromExtension("pdf") == ResourceType::pdfDoc);
        REQUIRE(resourceTypeFromExtension("epub") == ResourceType::epubDoc);
    }

    SECTION("Return nullopt check") {
        REQUIRE(resourceTypeFromExtension("jpg") == std::nullopt);
        REQUIRE(resourceTypeFromExtension("PDF") == std::nullopt);
        REQUIRE(resourceTypeFromExtension("") == std::nullopt);
        REQUIRE(resourceTypeFromExtension(".") == std::nullopt);
    }
}

TEST_CASE("resourceTypeFromFile", "[Model][Utils]") {
    SECTION("Check valid string file path") {
        REQUIRE(resourceTypeFromFile("/mnt/Path/resource.txt").value() == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromFile(R"(C:\res\exam.h)") == ResourceType::cCppCode);
        REQUIRE(resourceTypeFromFile("/mnt/MountPoint/path/res.pdf") == ResourceType::pdfDoc);
        REQUIRE(resourceTypeFromFile("D:\\cpp ebook\\learn.epub") == ResourceType::epubDoc);
    }

    SECTION("Check invalid string file path") {
        REQUIRE(resourceTypeFromFile("") == std::nullopt);
        REQUIRE(resourceTypeFromFile(".txt") == std::nullopt);
    }

    SECTION("File with no extension (Should be nullopt)") {
        // Đường dẫn không có extension
        REQUIRE(resourceTypeFromFile("/path/to/my_resource_file") == std::nullopt);
        // Đường dẫn Windows không có extension
        REQUIRE(resourceTypeFromFile(R"(C:\Docs\NoExtFile)") == std::nullopt);
    }

    SECTION("File with multiple dots (Only last part is extension)") {
        // Ví dụ: file.tar.gz -> ext là "gz"
        // Vì extMap không có "gz", nên phải là nullopt.
        // Cần đảm bảo Utils::getFileExtension trả về "gz" (hoặc "pdf" nếu tên file là
        // "archive.tar.pdf")

        // Giả sử tên file là "report.v1.pdf" -> ext là "pdf"
        REQUIRE(resourceTypeFromFile("/archive/report.v1.pdf") == ResourceType::pdfDoc);

        // Giả sử tên file là "archive.zip" -> ext là "zip" -> nullopt
        REQUIRE(resourceTypeFromFile(R"(C:\Data\archive.zip)") == std::nullopt);
    }

    SECTION("File path ending with a dot (Should be nullopt)") {
        // Đường dẫn kết thúc bằng dấu chấm: "/path/to/file." -> ext là ""
        REQUIRE(resourceTypeFromFile("/path/to/file.") == std::nullopt);
    }

    SECTION("Root paths (Should be nullopt)") {
        // Test các đường dẫn gốc không phải file
        REQUIRE(resourceTypeFromFile("/") == std::nullopt);
        REQUIRE(resourceTypeFromFile(R"(C:\)") == std::nullopt);
    }
}
