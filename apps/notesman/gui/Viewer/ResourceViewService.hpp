#pragma once

#include <optional>
#include <QString>
#include <sqlite3.h>

#include "NotesAppCore.hpp"

class ResourceViewService {
    public:
        explicit ResourceViewService(NotesAppCore& core);

        // LOAD
        [[nodiscard]] std::optional<QString> loadTextResource(sqlite3_int64 resourceId) const;

        // SAVE
        void saveTextResource(sqlite3_int64 resourceId, QString const& content) const;

    private:
        NotesAppCore& m_core;
};
