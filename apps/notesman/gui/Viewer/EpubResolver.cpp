#include "gui/Viewer/EpubResolver.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>
#include <Qt>
#include <QtTypes>
#include <optional>
#include <vector>
#include <zip.h>
#include <zipconf.h>

namespace {

void rewriteRelativeUrls(QString& html, QUrl const& baseUrl) {
    static QRegularExpression const re(R"((href|src)\s*=\s*["']([^"':#][^"']*)["'])",
                                       QRegularExpression::CaseInsensitiveOption);

    QString result;
    result.reserve(html.size());

    qsizetype lastPos{};
    auto it = re.globalMatch(html);

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();

        // copy text trước match
        result.append(html.mid(lastPos, m.capturedStart() - lastPos));

        QString const attr = m.captured(1);
        QString const path = m.captured(2);

        QUrl abs = baseUrl.resolved(QUrl(path));

        result.append(attr);
        result.append("=\"");
        result.append(abs.toString(QUrl::FullyEncoded));
        result.append('"');

        lastPos = m.capturedEnd();
    }

    // phần còn lại
    result.append(html.mid(lastPos));

    html.swap(result);
}

QString makeWorkDir(QString const& epubPath) {
    QByteArray const hash =
        QCryptographicHash::hash(epubPath.toUtf8(), QCryptographicHash::Sha256).toHex();

    QString const base =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/epub/" + hash;

    QDir{}.mkpath(base);
    return base;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool extractZip(QString const& zipPath, QString const& outDir) {
    int err = 0;
    zip* za = zip_open(zipPath.toUtf8().constData(), ZIP_RDONLY, &err);
    if (za == nullptr) {
        return false;
    }

    zip_int64_t const count = zip_get_num_entries(za, 0);

    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(count); ++i) {
        struct zip_stat st{};
        zip_stat_init(&st);
        if (zip_stat_index(za, i, 0, &st) != 0) {
            continue;
        }

        QString const name = QString::fromUtf8(st.name);
        QString const outPath = outDir + "/" + name;

        // directory
        if (name.endsWith('/')) {
            QDir{}.mkpath(outPath);
            continue;
        }

        QDir{}.mkpath(QFileInfo(outPath).path());

        zip_file* zf = zip_fopen_index(za, i, 0);
        if (zf == nullptr) {
            zip_close(za);
            return false;
        }

        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            zip_fclose(zf);
            zip_close(za);
            return false;
        }

        std::vector<char> buf(8192);
        zip_int64_t n = 0;
        while ((n = zip_fread(zf, buf.data(), buf.size())) > 0) {
            outFile.write(buf.data(), n);
        }

        outFile.close();
        zip_fclose(zf);
    }

    zip_close(za);
    return true;
}

std::optional<QString> findOpfPath(QString const& rootDir) {
    QFile file(rootDir + "/META-INF/container.xml");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "rootfile") {
            auto const attrs = xml.attributes();
            if (attrs.hasAttribute("full-path")) {
                return rootDir + "/" + attrs.value("full-path").toString();
            }
        }
    }

    return std::nullopt;
}

std::optional<EpubResolver::OpfData> parseOpf(QString const& opfPath) {
    QFile file(opfPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QString const baseDir = QFileInfo(opfPath).path();

    QXmlStreamReader xml(&file);

    QHash<QString, QString> manifest;
    std::vector<QString> spineIds;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            if (xml.name() == "item") {
                auto const a = xml.attributes();
                manifest.insert(a.value("id").toString(), a.value("href").toString());
            } else if (xml.name() == "itemref") {
                spineIds.push_back(xml.attributes().value("idref").toString());
            }
        }
    }

    EpubResolver::OpfData data;
    data.baseDir = baseDir;

    for (auto const& id : spineIds) {
        if (manifest.contains(id)) {
            data.spineFiles.push_back(baseDir + "/" + manifest[id]);
        }
    }

    if (data.spineFiles.empty()) {
        return std::nullopt;
    }

    return data;
}

QString generateEntryHtml(EpubResolver::OpfData const& opf, QString const& outDir) {
    QString const entryPath = outDir + "/__entry.html";
    QFile f(entryPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream ts(&f);
    ts << "<!doctype html>\n<html>\n<head>\n";
    ts << "<meta charset=\"utf-8\" />\n";
    ts << "<style>\n"
          "html, body { margin:0; padding:10px; }\n"
          ".epub-spine { margin-bottom: 2em; }\n"
          "</style>\n";
    ts << "</head>\n<body>\n";

    for (auto const& chapter : opf.spineFiles) {
        QFile cf(chapter);
        if (!cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QString content = QString::fromUtf8(cf.readAll());
        cf.close();

        // Cắt <body>
        auto const bodyStart = content.indexOf("<body", Qt::CaseInsensitive);
        if (bodyStart >= 0) {
            auto const bodyTagEnd = content.indexOf('>', bodyStart);
            auto const bodyEnd = content.lastIndexOf("</body>", Qt::CaseInsensitive);
            if (bodyTagEnd >= 0 && bodyEnd > bodyTagEnd) {
                content = content.mid(bodyTagEnd + 1, bodyEnd - bodyTagEnd - 1);
            }
        }

        // Rewrite URL
        QString const spineDir = QFileInfo(chapter).path();
        QUrl const spineBase = QUrl::fromLocalFile(spineDir + "/");

        rewriteRelativeUrls(content, spineBase);

        // Append
        ts << "<section class=\"epub-spine\">\n";
        ts << content << "\n";
        ts << "</section>\n";
    }

    ts << "</body>\n</html>\n";
    f.close();

    return QString{entryPath};
}

} // namespace

std::optional<QString> EpubResolver::resolveToHtml(QString const& epubPath) {
    QFileInfo fi(epubPath);
    if (!fi.exists() || fi.suffix().toLower() != "epub") {
        return std::nullopt;
    }

    QString const workDir = makeWorkDir(fi.absoluteFilePath());

    // extract only once
    if (!QFileInfo::exists(workDir + "/META-INF/container.xml")) {
        if (!extractZip(fi.absoluteFilePath(), workDir)) {
            return std::nullopt;
        }
    }

    auto const opfPath = findOpfPath(workDir);
    if (!opfPath) {
        return std::nullopt;
    }

    auto const opf = parseOpf(*opfPath);
    if (!opf) {
        return std::nullopt;
    }

    QString const entry = generateEntryHtml(*opf, workDir);
    if (entry.isEmpty()) {
        return std::nullopt;
    }

    return QFileInfo(entry).absoluteFilePath();
}
