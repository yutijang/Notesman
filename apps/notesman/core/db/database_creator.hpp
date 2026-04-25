#pragma once

#include "Logger.hpp"
#include "schema_version.hpp"
#include "sqldb_raii.hpp"

#include <exception>
#include <sqlite3.h>
#include <string>

class DatabaseCreator {
    public:
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        static bool create(std::string const& dbPath, std::string const& schemaSql,
                           std::string& error) {
            try {
                {
                    SQLiteDB db(dbPath);

                    char* errMsg{};
                    int rc = sqlite3_exec(db.get(), schemaSql.c_str(), nullptr, nullptr, &errMsg);

                    if (rc != SQLITE_OK) {
                        error = (errMsg != nullptr) ? errMsg : "Unknown error creating schema";
                        Log::fatal(error);
                        sqlite3_free(errMsg);
                        return false;
                    }

                    char* versionSql =
                        sqlite3_mprintf("PRAGMA user_version = %d;", app::meta::SCHEMA_VERSION);
                    rc = sqlite3_exec(db.get(), versionSql, nullptr, nullptr, &errMsg);
                    sqlite3_free(versionSql);

                    if (rc != SQLITE_OK) {
                        error = (errMsg != nullptr) ? errMsg : "Error setting user_version";
                        Log::fatal(error);
                        sqlite3_free(errMsg);
                        return false;
                    }

                    return true;
                }
            } catch (std::exception const& ex) {
                error = ex.what();
                return false;
            }
        }
};
