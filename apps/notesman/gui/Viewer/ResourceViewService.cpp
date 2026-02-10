#include <optional>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <sqlite3.h>

#include "NotesAppCore.hpp"
#include "ResourceViewService.hpp"
#include "model.hpp"

ResourceViewService::ResourceViewService(NotesAppCore &core) : m_core(core) {}

std::optional<QString> ResourceViewService::loadTextResource(sqlite3_int64 resourceId) const {
    const auto fullResOpt = m_core.getFullResource(resourceId);
    if (!fullResOpt) { return std::nullopt; }

    const auto &fullRes = *fullResOpt;

    if (fullRes.filepath.has_value()) {
        QFile file(QString::fromStdString(*fullRes.filepath));

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return std::nullopt; }

        QTextStream in(&file);
        return in.readAll();
    }

    if (fullRes.content.has_value()) { return QString::fromStdString(*fullRes.content); }

    return std::nullopt;
}

void ResourceViewService::saveTextResource(sqlite3_int64 resourceId, const QString &content) const {
    const auto fullResOpt = m_core.getFullResource(resourceId);
    if (!fullResOpt) { return; }

    const auto &fullRes = *fullResOpt;

    // ====== Chỉ text thuần mới được lưu ======
    if (fullRes.resource.type == ResourceType::PlainText) {
        m_core.updateText(resourceId, content.toStdString());
    }

    // ====== cpp / file-based resource: chỉ xem ======
}
