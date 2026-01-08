#include <ranges>
#include <string>
#include <vector>
#include <QObject>
#include <QStringView>

#include "ResourceSearchWorker.hpp"
#include "Logger.hpp"
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

    try {
        if (m_mode == "title" || m_mode == "content") {
            if (m_mode == "title") {
                const auto sanitizedKW = Utils::sanitizeFtsQuery(stdKeyword, true);
                results = m_core->searchByTitleFull(sanitizedKW);
            } else {
                const auto sanitizedKW = Utils::sanitizeFtsQuery(stdKeyword, false);
                results = m_core->searchByContentUnifiedFull(sanitizedKW);
            }
        } else if (m_mode == "tag") {
            std::string cleanTag = stdKeyword;
            std::erase(cleanTag, '\"');

            auto isSpace = [](unsigned char ch) { return std::isspace(ch); };
            auto trimmedView = cleanTag | std::views::drop_while(isSpace) | std::views::reverse |
                               std::views::drop_while(isSpace) | std::views::reverse;

            std::string trimmedS(trimmedView.begin(), trimmedView.end());

            results = m_core->getFullResourcesByTag(trimmedS);
        } else if (m_mode == "all") {
            // 1. Chuẩn bị chuỗi cho LIKE (Bảng Tag)
            std::string likeKW = Utils::toLikeQuery(stdKeyword);

            // 2. Chuẩn bị chuỗi cho MATCH (Bảng FTS Title/Content)
            // Dùng false để tìm kiếm linh hoạt hơn trong chế độ tìm tổng hợp
            std::string ftsKW = Utils::sanitizeFtsQuery(stdKeyword + "*", false);

            // 3. Gọi hàm hợp nhất trong Core
            // Hàm này sẽ bind likeKW vào các tham số LIKE và ftsKW vào các tham số MATCH
            results = m_core->searchUnifiedFull(likeKW, ftsKW);
        }
    } catch (const std::runtime_error &ex) { Log::err("Error: {}", ex.what()); }

    emit searchFinished(results);
}
