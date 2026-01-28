#include "MarkdownToHtml.hpp"

#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QByteArray>
#include <QDir>
#include <md4c.h>

extern "C" {
#include <md4c-html.h>
}

static void mdHtmlWrite(const MD_CHAR* data, MD_SIZE size, void* userdata) {
    auto* out = static_cast<QByteArray*>(userdata);
    out->append(reinterpret_cast<const char*>(data), static_cast<int>(size));
}

QString MarkdownToHtml::convertFileToHtml(const QString &mdPath) {
    QFile f(mdPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }

    const QByteArray md = f.readAll();

    QByteArray htmlBody;
    const int rc = md_html(md.constData(), static_cast<MD_SIZE>(md.size()), mdHtmlWrite, &htmlBody,
                           MD_FLAG_TABLES | MD_FLAG_TASKLISTS, 0);

    if (rc != 0) { return {}; }

    // wrap HTML document
    QByteArray html;
    html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    html += "<style>body{font-family:sans-serif;padding:1em;}</style>";
    html += "</head><body>";
    html += htmlBody;
    html += "</body></html>";

    return QString::fromUtf8(html);
}
