#pragma once

#include <string>
#include <optional>
#include <sqlite3.h>
#include "model.hpp"

class SQLiteDB;
class FileRepository;
class ResourceRepository;

class FileService {
    public:
        FileService(SQLiteDB &db, FileRepository &fileRepo, ResourceRepository &resRepo) noexcept
            : m_db(db), m_fileRepo(fileRepo), m_resRepo(resRepo) {}

        // Tính hash file (SHA256)
        static std::string computeFileHash(const std::string &filePath);

        // Thêm file vào DB kèm hash
        // filepath: đường dẫn gốc user chọn
        // title: tiêu đề resource
        // type: loại resource (pdf, epub,...)
        // isManaged: true = copy vào storage, false = chỉ link ngoài
        sqlite3_int64 addFileResource(const std::string &filepath, const std::string &title,
                                      ResourceType type, bool isManaged);

        // Kiểm tra file đã được index chưa
        std::optional<sqlite3_int64> findResourceByFile(const std::string &filepath);

        // Đồng bộ lại hash (khi file thay đổi nội dung)
        void refreshFileHash(sqlite3_int64 resourceId);

    private:
        SQLiteDB &m_db;
        FileRepository &m_fileRepo;
        ResourceRepository &m_resRepo;

        // Helper: copy file vào storage (nếu isManaged = true)
        static std::string copyToStorage(const std::string &srcPath, const std::string &hash);
};
