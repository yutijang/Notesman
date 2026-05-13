#include "MarkdownToHtml.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>
#include <md4c-html.h>
#include <md4c.h>

namespace {

QString readResource(QString const& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return file.readAll();
}

void mdHtmlWrite(const MD_CHAR* data, MD_SIZE size, void* userdata) {
    auto* out = static_cast<QByteArray*>(userdata);
    out->append(reinterpret_cast<char const*>(data), static_cast<int>(size));
}

} // namespace

QString MarkdownToHtml::convertFileToHtml(QString const& mdPath, bool isDarkTheme) {
    QFileInfo mdFi(mdPath);
    if (!mdFi.exists() || !mdFi.isFile()) {
        return {};
    }

    // ---- cache dir ----
    QDir const cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                        "/markdown");
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    // ---- cache file name (path + theme) ----
    QByteArray const key = (mdFi.absoluteFilePath() + (isDarkTheme ? "|dark" : "|light")).toUtf8();
    QString const hash = QString::number(qHash(key), 16);
    QString const htmlPath = cacheDir.absoluteFilePath(hash + ".html");

    QFileInfo htmlFi(htmlPath);

    // ---- reuse cache nếu hợp lệ ----
    if (htmlFi.exists() && htmlFi.lastModified() >= mdFi.lastModified()) {
        return htmlPath;
    }

    QFile f(mdPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QByteArray const md = f.readAll();
    QByteArray htmlBody;
    unsigned const parserFlags = MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_STRIKETHROUGH |
                                 MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_PERMISSIVEATXHEADERS |
                                 MD_FLAG_LATEXMATHSPANS;

    int const rc = md_html(
        md.constData(), static_cast<MD_SIZE>(md.size()), mdHtmlWrite, &htmlBody, parserFlags, 0);

    if (rc != 0) {
        return {};
    }

    QUrl const baseUrl = QUrl::fromLocalFile(mdFi.absolutePath() + "/");

    QByteArray html;
    html += isDarkTheme ? R"(<!DOCTYPE html><html style="color-scheme: dark;">)"
                        : R"(<!DOCTYPE html><html style="color-scheme: light;">)";

    html += R"(<head><meta charset="utf-8">)";
    html += R"(<meta name="viewport" content="width=device-width, initial-scale=1">)";

    html += "<base href=\"" + baseUrl.toString(QUrl::FullyEncoded).toUtf8() + "\">";

    html += "<style>";

    if (isDarkTheme) {
        html += readResource(":/md_assets/github-markdown-dark.min.css").toUtf8();
        html += readResource(":/md_assets/github-dark.min.css").toUtf8();
    } else {
        html += readResource(":/md_assets/github-markdown-light.min.css").toUtf8();
        html += readResource(":/md_assets/github.min.css").toUtf8();
    }

    html += "html, body { min-height: 100%; margin: 0; }";

    if (isDarkTheme) {
        html += "body { background-color: #0d1117; }";
        html += ".markdown-body pre code { color: #c9d1d9 !important; }";
        html += ".markdown-body pre { background-color: #161b22 !important; }";
    } else {
        html += "body { background-color: #ffffff; }";
    }

    html += ".markdown-body { font-family: -apple-system, BlinkMacSystemFont, "
            "Segoe UI, Helvetica, Arial, sans-serif; }";

    html += ".markdown-body { box-sizing: border-box; min-width: 200px; max-width: 980px; ";
    html += "margin: 0 auto; padding: 45px; background-color: transparent !important; }";

    html += "@media (max-width: 767px) { .markdown-body { padding: 15px; } }";
    html += "</style></head><body>";

    html += R"(<div class="markdown-body">)";
    html += htmlBody;
    html += "</div>";

    html += "<script>";
    html += readResource(":/md_assets/highlight.min.js").toUtf8();
    html += "</script>";

    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', (event) => {";
    html += "  document.querySelectorAll('pre code').forEach((el) => {";
    html += "    hljs.highlightElement(el);";
    html += "  });";
    html += "});";
    html += "</script>";

    html += "</body></html>";

    // ---- ghi file cache ----
    QFile out(htmlPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }

    out.write(html);
    out.close();

    return htmlPath;
}
