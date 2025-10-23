#include <iomanip>
#include <iostream>
#include <string>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <array>
#include <filesystem>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "file_service.hpp"
#include "file_repository.hpp"
#include "model.hpp"
#include "resource_repository.hpp"

// Tính hash file (SHA256)
std::string FileService::computeFileHash(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) { throw std::runtime_error("Error, cannot open file: " + filePath); }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) { throw std::runtime_error("Failed create EVP_MD_CTX"); }

    const EVP_MD* md = EVP_sha256();
    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    const int bufferSize{8192};
    std::array<char, bufferSize> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(file.gcount())) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestUpdate failed");
        }
    }

    // unsigned char hash[EVP_MAX_MD_SIZE];
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashLen{};

    if (EVP_DigestFinal_ex(ctx, hash.data(), &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hashLen; ++i) { oss << std::setw(2) << static_cast<int>(hash[i]); }

    return oss.str();
}

// Thêm file vào DB kèm hash
// NOLINTNEXTLINE
sqlite3_int64 FileService::addFileResource(const std::string &filepath, const std::string &title,
                                           ResourceType type, bool isManaged) {
    // Tính hash của file
    std::string hash = computeFileHash(filepath);

    // Kiểm tra hash có tồn tại chưa
    auto existing = m_resRepo.getByFileHash(hash);
    if (existing.has_value()) { return existing->id; } // đã tồn tại -> trả về resource_id

    std::string storedPath;
    if (isManaged) {
        storedPath = copyToStorage(filepath, hash);
    } else {
        storedPath = filepath;
    }

    sqlite3_int64 resourceId =
        m_resRepo.insert({.title = title, .type = type, .file_hash = hash}); // NOLINT

    m_fileRepo.insertFile(resourceId, storedPath, filepath, isManaged);

    return resourceId;
}

// Kiểm tra file đã được index chưa
std::optional<sqlite3_int64> FileService::findResourceByFile(const std::string &filepath) {
    // Kiểm tra trước với original_path
    auto byOriginal = m_fileRepo.getResourceIdByOriginalPath(filepath);
    if (byOriginal.has_value()) { return byOriginal; }

    // Kiểm tra theo hash
    std::string hash = computeFileHash(filepath);
    auto byHash = m_resRepo.getByFileHash(hash);
    if (byHash.has_value()) { return byHash->id; }

    return std::nullopt;
}

// Đồng bộ lại hash (khi file thay đổi nội dung)
void FileService::refreshFileHash(sqlite3_int64 resourceId) {
    auto fileEntryOpt = m_fileRepo.getFileById(resourceId);
    if (!fileEntryOpt.has_value()) {
        throw std::runtime_error("No file entry for resource ID: " + std::to_string(resourceId));
    }

    const auto &entry = *fileEntryOpt;
    if (!entry.stored_path.has_value()) {
        throw std::runtime_error("File has no stored_path for resource ID: " +
                                 std::to_string(resourceId));
    }

    std::string newHash = computeFileHash(*entry.stored_path);
    m_resRepo.updateFileHash(resourceId, newHash);
}

// NOLINTNEXTLINE
std::string FileService::copyToStorage(const std::string &srcPath, const std::string &hash) {
    namespace fs = std::filesystem;

    fs::path storageDir = "resources";
    if (!fs::exists(storageDir)) { fs::create_directories(storageDir); }

    fs::path ext = fs::path(srcPath).extension();
    fs::path dest = storageDir / (hash + ext.string());

    fs::copy_file(srcPath, dest, fs::copy_options::skip_existing);

    if (!fs::exists(storageDir)) { std::cout << "path not exist"; } // here

    return dest.string();
}
