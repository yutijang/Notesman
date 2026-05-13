#include "ResourceViewerFactory.hpp"

#include "ContentMode.hpp"
#include "EpubResolver.hpp"
#include "HtmlViewer.hpp"
#include "IResourceViewer.hpp"
#include "Logger.hpp"
#include "MarkdownToHtml.hpp"
#include "PdfViewer.hpp"
#include "ResourceViewService.hpp"
#include "TextViewer.hpp"
#include "UiConstants.hpp"
#include "model.hpp"

#include <QString>
#include <QUrl>
#include <cstdint>
#include <memory>
#include <sqlite3.h>

auto ResourceViewerFactory::create(std::int64_t id,
                                   ResourceType type,
                                   QString const& title,
                                   QString const& path,
                                   QString const& url,
                                   UiConst::Theme theme,
                                   ResourceViewService& viewService,
                                   QWidget* parent) -> std::unique_ptr<IResourceViewer> {
    bool const isDarkTheme = (theme == UiConst::Theme::Dark);
    std::unique_ptr<IResourceViewer> viewer;

    switch (type) {
        case ResourceType::PlainText:
        case ResourceType::CCppCode : {
            bool const editable = (type == ResourceType::PlainText && path.isEmpty());
            viewer = std::make_unique<TextViewer>(
                static_cast<sqlite3_int64>(id), editable, viewService, theme, parent);
            break;
        }
        case ResourceType::HtmlDoc:
        case ResourceType::Markdown: {
            if (type == ResourceType::Markdown) {
                QString const htmlFileFromMd = MarkdownToHtml::convertFileToHtml(path, isDarkTheme);
                if (htmlFileFromMd.isEmpty()) {
                    return nullptr;
                }
                viewer = HtmlViewer::createFromFile(
                    title, htmlFileFromMd, ContentMode::HtmlFile, parent);
            } else {
                viewer = HtmlViewer::createFromFile(title, path, ContentMode::HtmlFile, parent);
            }

            if (!viewer) {
                Log::err("Invalid WebView2 runtime");
                return nullptr;
            }
            break;
        }
        case ResourceType::Url: {
            QUrl const qurl = QUrl::fromUserInput(url);
            viewer = HtmlViewer::createFromUrl(title, qurl, ContentMode::Url, parent);
            if (!viewer) {
                Log::err("Invalid WebView2 runtime");
                return nullptr;
            }
            break;
        }
        case ResourceType::PdfDoc: {
            viewer = std::make_unique<PdfViewer>(path, parent);
            break;
        }
        case ResourceType::EpubDoc: {
            auto epubResolvedPathOpt = EpubResolver::resolveToHtml(path);
            if (!epubResolvedPathOpt) {
                return nullptr;
            }
            viewer =
                HtmlViewer::createFromFile(title, *epubResolvedPathOpt, ContentMode::Epub, parent);
            if (!viewer) {
                Log::err("Invalid WebView2 runtime");
                return nullptr;
            }
            break;
        }
        case ResourceType::Unknown:
        case ResourceType::Count  : break;
    }

    return viewer;
}
