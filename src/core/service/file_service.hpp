#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <sqlite3.h>

#include "model.hpp"
#include "sqldb_raii.hpp"
#include "file_repository.hpp"
#include "resource_repository.hpp"

class FileService {
    public:
        FileService(SQLiteDB &db, FileRepository &fileRepo, ResourceRepository &resRepo) noexcept
            : m_db(db), m_fileRepo(fileRepo), m_resRepo(resRepo) {}

        // Tính hash file (SHA256)
        static std::string computeFileHash(const std::filesystem::path &filePath);

        // Thêm file vào DB kèm hash
        // filepath: đường dẫn gốc user chọn
        // title: tiêu đề resource
        // type: loại resource (pdf, epub,...)
        // isManaged: true = copy vào storage, false = chỉ link ngoài
        sqlite3_int64 addFileResource(const std::filesystem::path &filepath,
                                      const std::string &title, ResourceType type, bool isManaged);

        // Kiểm tra file đã được index chưa
        std::optional<sqlite3_int64> findResourceByFile(const std::filesystem::path &filepath);

        // Đồng bộ lại hash (khi file thay đổi nội dung)
        void refreshFileHash(sqlite3_int64 resourceId);

    private:
        // Helper: copy file vào storage (nếu isManaged = true)
        std::filesystem::path copyToStorage(const std::filesystem::path &srcPath,
                                            const std::string &hash);

        SQLiteDB &m_db;
        FileRepository &m_fileRepo;
        ResourceRepository &m_resRepo;
};
