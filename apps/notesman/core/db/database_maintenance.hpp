#pragma once

#include <sqlite3.h>
#include <stdexcept>
#include <string>

class DatabaseMaintenance {
    public:
        static void compact(std::string const& dbPath) {
            sqlite3* db{};

            int rc = sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr);
            if (rc != SQLITE_OK) {
                std::string err = ((db != nullptr) ? sqlite3_errmsg(db) : "unknown");
                if (db != nullptr) { sqlite3_close_v2(db); }
                throw std::runtime_error("Cannot open database: " + err);
            }

            char* errMsg{};
            rc = sqlite3_exec(db, "VACUUM;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                std::string msg = (errMsg != nullptr) ? errMsg : "unknown";
                sqlite3_free(errMsg);
                sqlite3_close_v2(db);
                throw std::runtime_error("VACUUM failed: " + msg);
            }

            sqlite3_close_v2(db);
        }
};
