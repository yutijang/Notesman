#include <QObject>
#include <QStringView>

#include "ResourceSearchWorker.hpp"
#include "NotesAppCore.hpp"
#include "model.hpp"

void ResourceSearchWorker::doSearch() {
    if (m_core == nullptr) {
        emit searchFinished({});
        return;
    }

    std::vector<FullResource> results;

    if (m_mode == "title") {
        results = m_core->searchByTitleFull(m_keyword.toUtf8().toStdString());
    } else if (m_mode == "content") {
        results = m_core->searchByContentFull(m_keyword.toUtf8().toStdString());
    } else if (m_mode == "tag") {
        results = m_core->getFullResourcesByTag(m_keyword.toUtf8().toStdString());
    }

    emit searchFinished(results);
}
