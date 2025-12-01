#pragma once

#include <string>
#include <stdexcept>
#include <sqlite3.h>

class DatabaseMaintenance {
    public:
        static void compact(const std::string &dbPath) {
            sqlite3* db = nullptr;

            int rc = sqlite3_open_v2(dbPath.c_str(), &db,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            if (rc != SQLITE_OK) {
                std::string err = ((db != nullptr) ? sqlite3_errmsg(db) : "unknown");
                if (db != nullptr) { sqlite3_close_v2(db); }
                throw std::runtime_error("Cannot open database: " + err);
            }

            char* errMsg = nullptr;
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
