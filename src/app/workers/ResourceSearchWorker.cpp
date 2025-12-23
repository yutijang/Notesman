#include <QObject>
#include <QStringView>

#include "ResourceSearchWorker.hpp"
#include "NotesAppCore.hpp"
#include "model.hpp"
#include "helper.hpp"

void ResourceSearchWorker::doSearch() {
    if (m_core == nullptr) {
        emit searchFinished({});
        return;
    }

    std::string stdKeyword = m_keyword.toUtf8().toStdString();
    std::vector<FullResource> results;

    if (m_mode == "title" || m_mode == "content") {
        const auto sanitizedKW = Utils::sanitizeFtsQuery(stdKeyword);

        if (m_mode == "title") {
            results = m_core->searchByTitleFull(sanitizedKW);
        } else {
            results = m_core->searchByContentFull(sanitizedKW);
        }
    } else if (m_mode == "tag") {
        results = m_core->getFullResourcesByTag(stdKeyword);
    }

    emit searchFinished(results);
}
