#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QByteArray>
#include <QDir>
#include <md4c.h>
#include <md4c-html.h>

#include "MarkdownToHtml.hpp"

static void mdHtmlWrite(const MD_CHAR* data, MD_SIZE size, void* userdata) {
    auto* out = static_cast<QByteArray*>(userdata);
    out->append(reinterpret_cast<const char*>(data), static_cast<int>(size));
}

QString MarkdownToHtml::convertFileToHtml(const QString &mdPath, bool isDarkTheme) {
    QFile f(mdPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }

    const QByteArray md = f.readAll();
    QByteArray htmlBody;
    const unsigned parserFlags = MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_STRIKETHROUGH |
                                 MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_PERMISSIVEATXHEADERS |
                                 MD_FLAG_LATEXMATHSPANS;

    const int rc = md_html(md.constData(), static_cast<MD_SIZE>(md.size()), mdHtmlWrite, &htmlBody,
                           parserFlags, 0);

    if (rc != 0) { return {}; }

    QByteArray html;
    html += isDarkTheme ? R"(<!DOCTYPE html><html style="color-scheme: dark;">)"
                        : R"(<!DOCTYPE html><html style="color-scheme: light;">)";

    html += R"(<head><meta charset="utf-8">)";
    html += R"(<meta name="viewport" content="width=device-width, initial-scale=1">)";

    if (isDarkTheme) {
        html +=
            R"(<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/github-markdown-css/5.5.1/github-markdown-dark.min.css">)";
        html +=
            R"(<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css">)";
    } else {
        html +=
            R"(<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/github-markdown-css/5.5.1/github-markdown-light.min.css">)";
        html +=
            R"(<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css">)";
    }

    html += "<style>";
    html += "html, body { min-height: 100%; margin: 0; }";

    if (isDarkTheme) {
        html += "body { background-color: #0d1117; }";
        html += ".markdown-body pre code { color: #c9d1d9 !important; }";
        html += ".markdown-body pre { background-color: #161b22 !important; }";
    } else {
        html += "body { background-color: #ffffff; }";
    }

    html += ".markdown-body { box-sizing: border-box; min-width: 200px; max-width: 980px; ";
    html += "margin: 0 auto; padding: 45px; background-color: transparent !important; }";

    html += "@media (max-width: 767px) { .markdown-body { padding: 15px; } }";
    html += "</style></head><body>";

    html += R"(<div class="markdown-body">)";
    html += htmlBody;
    html += "</div>";

    html +=
        R"(<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>)";
    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', (event) => {";
    html += "  document.querySelectorAll('pre code').forEach((el) => {";
    html += "    hljs.highlightElement(el);";
    html += "  });";
    html += "});";
    html += "</script>";

    html += "</body></html>";

    return QString::fromUtf8(html);
}
