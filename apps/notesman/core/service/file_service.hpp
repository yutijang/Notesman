#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <sqlite3.h>

#include "model.hpp"
#include "file_repository.hpp"
#include "resource_repository.hpp"
#include "file_text_content_repository.hpp"

class FileService {
    public:
        FileService(FileRepository &fileRepo, ResourceRepository &resRepo,
                    FileTextContentRepository &fileTextRepo) noexcept
            : m_fileRepo(fileRepo), m_resRepo(resRepo), m_fileTextRepo(fileTextRepo) {}

        // Tính hash file (SHA256)
        static std::string computeFileHash(const std::filesystem::path &filePath);

        // Thêm file vào DB kèm hash
        // filepath: đường dẫn gốc user chọn
        // title: tiêu đề resource
        // type: loại resource (pdf, epub,...)
        // isManaged: true = copy vào storage, false = chỉ link ngoài
        sqlite3_int64 addFileResource(const std::filesystem::path &filepath,
                                      const std::string &title, ResourceType type, bool isManaged,
                                      const std::string &contentToIndex);

        // Kiểm tra file đã được index chưa
        std::optional<sqlite3_int64> findResourceByFile(const std::filesystem::path &filepath);

        // Đồng bộ lại hash (khi file thay đổi nội dung)
        void refreshFileHash(sqlite3_int64 resourceId);

    private:
        // Helper: copy file vào storage (nếu isManaged = true)
        static std::filesystem::path copyToStorage(const std::filesystem::path &srcPath,
                                                   const std::string &hash);

        FileRepository &m_fileRepo;
        ResourceRepository &m_resRepo;
        FileTextContentRepository &m_fileTextRepo;
};
