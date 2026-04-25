#pragma once

#include "model.hpp"

#include <QObject>
#include <QString>
#include <vector>

class NotesAppCore;

class ResourceSearchWorker final : public QObject {
        Q_OBJECT

    public:
        explicit ResourceSearchWorker(NotesAppCore* core, QObject* parent = nullptr)
            : QObject(parent), m_core(core) {}

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void setSearchParams(QString const& keyword, QString const& mode) {
            m_keyword = keyword.trimmed();
            m_mode = mode;
        }

        void doSearch();

    Q_SIGNALS:
        void searchFinished(std::vector<UnifiedSearchResult> const& results);

    private:
        NotesAppCore* m_core{};
        QString m_keyword;
        QString m_mode;
};
