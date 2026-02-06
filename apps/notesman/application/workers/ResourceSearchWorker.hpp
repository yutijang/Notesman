#pragma once

#include <QObject>
#include <QString>
#include <vector>

#include "model.hpp"

class NotesAppCore;

class ResourceSearchWorker final : public QObject {
        Q_OBJECT

    public:
        explicit ResourceSearchWorker(NotesAppCore* core, QObject* parent = nullptr)
            : QObject(parent), m_core(core) {}

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void setSearchParams(const QString &keyword, const QString &mode) {
            m_keyword = keyword.trimmed();
            m_mode = mode;
        }

        void doSearch();

    Q_SIGNALS:
        void searchFinished(const std::vector<UnifiedSearchResult> &results);

    private:
        NotesAppCore* m_core{};
        QString m_keyword;
        QString m_mode;
};
