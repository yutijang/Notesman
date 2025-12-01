#pragma once

#include <string>
#include <exception>
#include <sqlite3.h>

#include "sqldb_raii.hpp"

class DatabaseCreator {
    public:
        // NOLINTNEXTLINE
        static bool create(const std::string &dbPath, const std::string &schemaSql,
                           std::string &error) {
            try {
                {
                    SQLiteDB db(dbPath);

                    char* errMsg{};
                    int rc = sqlite3_exec(db.get(), schemaSql.c_str(), nullptr, nullptr, &errMsg);

                    if (rc != SQLITE_OK) {
                        error = (errMsg != nullptr) ? errMsg : "unknow";
                        sqlite3_free(errMsg);
                        return false;
                    }
                }
            } catch (const std::exception &ex) {
                error = ex.what();
                return false;
            }

            return true;
        }
};
