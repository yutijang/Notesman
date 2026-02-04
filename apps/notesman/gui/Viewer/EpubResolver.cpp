#include <vector>
#include <zip.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include "EpubResolver.hpp"

namespace {
    void rewriteRelativeUrls(QString &html, const QUrl &baseUrl) {
        static const QRegularExpression re(R"((href|src)\s*=\s*["']([^"':#][^"']*)["'])",
                                           QRegularExpression::CaseInsensitiveOption);

        QString result;
        result.reserve(html.size());

        qsizetype lastPos{};
        auto it = re.globalMatch(html);

        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();

            // copy text trước match
            result.append(html.mid(lastPos, m.capturedStart() - lastPos));

            const QString attr = m.captured(1);
            const QString path = m.captured(2);

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

    QString makeWorkDir(const QString &epubPath) {
        const QByteArray hash =
            QCryptographicHash::hash(epubPath.toUtf8(), QCryptographicHash::Sha256).toHex();

        const QString base =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/epub/" + hash;

        QDir{}.mkpath(base);
        return base;
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    bool extractZip(const QString &zipPath, const QString &outDir) {
        int err = 0;
        zip* za = zip_open(zipPath.toUtf8().constData(), ZIP_RDONLY, &err);
        if (za == nullptr) { return false; }

        const zip_int64_t count = zip_get_num_entries(za, 0);

        for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(count); ++i) {
            struct zip_stat st{};
            zip_stat_init(&st);
            if (zip_stat_index(za, i, 0, &st) != 0) { continue; }

            const QString name = QString::fromUtf8(st.name);
            const QString outPath = outDir + "/" + name;

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

    std::optional<QString> findOpfPath(const QString &rootDir) {
        QFile file(rootDir + "/META-INF/container.xml");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return std::nullopt; }

        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "rootfile") {
                const auto attrs = xml.attributes();
                if (attrs.hasAttribute("full-path")) {
                    return rootDir + "/" + attrs.value("full-path").toString();
                }
            }
        }

        return std::nullopt;
    }

    std::optional<EpubResolver::OpfData> parseOpf(const QString &opfPath) {
        QFile file(opfPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return std::nullopt; }

        const QString baseDir = QFileInfo(opfPath).path();

        QXmlStreamReader xml(&file);

        QHash<QString, QString> manifest;
        std::vector<QString> spineIds;

        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isStartElement()) {
                if (xml.name() == "item") {
                    const auto a = xml.attributes();
                    manifest.insert(a.value("id").toString(), a.value("href").toString());
                } else if (xml.name() == "itemref") {
                    spineIds.push_back(xml.attributes().value("idref").toString());
                }
            }
        }

        EpubResolver::OpfData data;
        data.baseDir = baseDir;

        for (const auto &id : spineIds) {
            if (manifest.contains(id)) { data.spineFiles.push_back(baseDir + "/" + manifest[id]); }
        }

        if (data.spineFiles.empty()) { return std::nullopt; }

        return data;
    }

    QString generateEntryHtml(const EpubResolver::OpfData &opf, const QString &outDir) {
        const QString entryPath = outDir + "/__entry.html";
        QFile f(entryPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }

        QTextStream ts(&f);
        ts << "<!doctype html>\n<html>\n<head>\n";
        ts << "<meta charset=\"utf-8\" />\n";
        ts << "<style>\n"
              "html, body { margin:0; padding:10px; }\n"
              ".epub-spine { margin-bottom: 2em; }\n"
              "</style>\n";
        ts << "</head>\n<body>\n";

        for (const auto &chapter : opf.spineFiles) {
            QFile cf(chapter);
            if (!cf.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }

            QString content = QString::fromUtf8(cf.readAll());
            cf.close();

            // Cắt <body>
            const auto bodyStart = content.indexOf("<body", Qt::CaseInsensitive);
            if (bodyStart >= 0) {
                const auto bodyTagEnd = content.indexOf('>', bodyStart);
                const auto bodyEnd = content.lastIndexOf("</body>", Qt::CaseInsensitive);
                if (bodyTagEnd >= 0 && bodyEnd > bodyTagEnd) {
                    content = content.mid(bodyTagEnd + 1, bodyEnd - bodyTagEnd - 1);
                }
            }

            // Rewrite URL
            const QString spineDir = QFileInfo(chapter).path();
            const QUrl spineBase = QUrl::fromLocalFile(spineDir + "/");

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

std::optional<QString> EpubResolver::resolveToHtml(const QString &epubPath) {
    QFileInfo fi(epubPath);
    if (!fi.exists() || fi.suffix().toLower() != "epub") { return std::nullopt; }

    const QString workDir = makeWorkDir(fi.absoluteFilePath());

    // extract only once
    if (!QFileInfo::exists(workDir + "/META-INF/container.xml")) {
        if (!extractZip(fi.absoluteFilePath(), workDir)) { return std::nullopt; }
    }

    const auto opfPath = findOpfPath(workDir);
    if (!opfPath) { return std::nullopt; }

    const auto opf = parseOpf(*opfPath);
    if (!opf) { return std::nullopt; }

    const QString entry = generateEntryHtml(*opf, workDir);
    if (entry.isEmpty()) { return std::nullopt; }

    return QFileInfo(entry).absoluteFilePath();
}
