#pragma once

#include "file_repository.hpp"
#include "file_text_content_repository.hpp"
#include "model.hpp"
#include "resource_repository.hpp"

#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>

class FileService {
    public:
        FileService(FileRepository& fileRepo, ResourceRepository& resRepo,
                    FileTextContentRepository& fileTextRepo) noexcept
            : m_fileRepo(fileRepo), m_resRepo(resRepo), m_fileTextRepo(fileTextRepo) {}

        // Tính hash file (SHA256)
        static std::string computeFileHash(std::filesystem::path const& filePath);

        // Thêm file vào DB kèm hash
        // filepath: đường dẫn gốc user chọn
        // title: tiêu đề resource
        // type: loại resource (pdf, epub,...)
        // isManaged: true = copy vào storage, false = chỉ link ngoài
        sqlite3_int64 addFileResource(std::filesystem::path const& filepath,
                                      std::string const& title, ResourceType type, bool isManaged,
                                      std::string const& contentToIndex);

        // Kiểm tra file đã được index chưa
        std::optional<sqlite3_int64> findResourceByFile(std::filesystem::path const& filepath);

        // Đồng bộ lại hash (khi file thay đổi nội dung)
        void refreshFileHash(sqlite3_int64 resourceId);

    private:
        // Helper: copy file vào storage (nếu isManaged = true)
        static std::filesystem::path copyToStorage(std::filesystem::path const& srcPath,
                                                   std::string const& hash);

        FileRepository& m_fileRepo;
        ResourceRepository& m_resRepo;
        FileTextContentRepository& m_fileTextRepo;
};
