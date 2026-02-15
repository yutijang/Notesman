#pragma once

#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <source_location>
#include <string_view>

#include "Logger.hpp"

namespace sqlite {
    inline void checkBind(int rc, sqlite3* db,
                          std::source_location loc = std::source_location::current()) {
        if (rc != SQLITE_OK) {
            char const* err = sqlite3_errmsg(db);
            Log::err(Log::SourceLocFmt{"SQLite bind failed: {}", loc}, err);
            throw std::runtime_error(std::string{"SQLite bind failed: "} + err);
        }
    }

    inline void checkStep(int rc, sqlite3* db, int expected, std::string_view context,
                          std::source_location loc = std::source_location::current()) {
        if (rc != expected) {
            std::string msg = sqlite3_errmsg(db);
            Log::err({context, loc}, "SQLite step failed (rc={}): {}", rc, msg);
            throw std::runtime_error(std::string(context) + ": " + msg);
        }
    }

    inline void checkExec(sqlite3* db, char const* sqlString, std::string_view context,
                          std::source_location loc = std::source_location::current()) {
        char* errMsg{};
        int rc = sqlite3_exec(db, sqlString, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::string finalErrMsg = (errMsg != nullptr) ? errMsg : "Unknown error";

            if (errMsg != nullptr) { sqlite3_free(errMsg); }

            Log::err({context, loc}, "SQLite exec failed (rc={}): {}", rc, finalErrMsg);
            throw std::runtime_error(std::string(context) + ": " + finalErrMsg);
        }
    }
} // namespace sqlite
