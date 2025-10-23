#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QPalette>
#include <QColor>
#include "cpphighlighter.hpp"

CppHighlighter::CppHighlighter(QTextDocument* parent, const CppHighlighterTheme &theme)
    : QSyntaxHighlighter(parent), m_theme(theme) {
    initRules();
}

CppHighlighter::~CppHighlighter() {
    stopGradualRehighlight();
}

void CppHighlighter::setTheme(const CppHighlighterTheme &theme) {
    m_theme = theme;
    initRules();
    // rehighlight();
}

void CppHighlighter::initRules() {
    m_rules.clear();
    m_stringRules.clear();

    // set formats
    // m_keywordFormat.setForeground(m_theme.keyword);
    m_flowKwFormat.setForeground(m_theme.flowKwFormat);
    m_builtinKwFormat.setForeground(m_theme.builtinKwFormat);
    m_typeFormat.setForeground(m_theme.type);
    m_stringFormat.setForeground(m_theme.string);
    m_commentFormat.setForeground(m_theme.comment);
    m_numberFormat.setForeground(m_theme.number);
    m_preprocessorFormat.setForeground(m_theme.preprocessor);
    m_functionFormat.setForeground(m_theme.function);
    m_identifierFormat.setForeground(m_theme.identifier);
    m_delimiterFormat.setForeground(m_theme.delimiter);

    // Preprocessor (kept as normal rule)
    m_rules.push_back(
        {.pattern = QRegularExpression(
             R"(^\s*#\s*(include|define|if|ifdef|ifndef|endif|pragma|error|warning)\b)"),
         .format = m_preprocessorFormat});

    // --- PHÂN NHÓM ---
    const QStringList flowKeywords = {"for",      "if",     "else",   "do",      "while", "break",
                                      "continue", "switch", "case",   "return",  "using", "try",
                                      "catch",    "new",    "delete", "default", "throw"};

    const QStringList builtinKeywords = {"alignas",
                                         "alignof",
                                         "and",
                                         "and_eq",
                                         "asm",
                                         "auto",
                                         "bitand",
                                         "bitor",
                                         "bool",
                                         "char",
                                         "char8_t",
                                         "char16_t",
                                         "char32_t",
                                         "class",
                                         "compl",
                                         "concept",
                                         "const",
                                         "consteval",
                                         "constexpr",
                                         "constinit",
                                         "const_cast",
                                         "co_await",
                                         "co_return",
                                         "co_yield",
                                         "decltype",
                                         "double",
                                         "dynamic_cast",
                                         "enum",
                                         "explicit",
                                         "export",
                                         "extern",
                                         "false",
                                         "float",
                                         "friend",
                                         "goto",
                                         "inline",
                                         "int",
                                         "long",
                                         "mutable",
                                         "namespace",
                                         "noexcept",
                                         "not",
                                         "not_eq",
                                         "nullptr",
                                         "operator",
                                         "or",
                                         "or_eq",
                                         "private",
                                         "protected",
                                         "public",
                                         "reflexpr",
                                         "register",
                                         "reinterpret_cast",
                                         "requires",
                                         "short",
                                         "signed",
                                         "sizeof",
                                         "static",
                                         "static_assert",
                                         "static_cast",
                                         "struct",
                                         "template",
                                         "this",
                                         "thread_local",
                                         "true",
                                         "typedef",
                                         "typeid",
                                         "typename",
                                         "union",
                                         "unsigned",
                                         "virtual",
                                         "void",
                                         "volatile",
                                         "wchar_t",
                                         "xor",
                                         "xor_eq"};

    // --- Áp dụng ---
    for (const auto &word : flowKeywords) {
        m_rules.push_back(
            {.pattern = QRegularExpression("\\b" + word + "\\b"), .format = m_flowKwFormat});
    }

    for (const auto &word : builtinKeywords) {
        m_rules.push_back(
            {.pattern = QRegularExpression("\\b" + word + "\\b"), .format = m_builtinKwFormat});
    }

    // Types (common)
    const QStringList types = {"std::string",     "std::vector", "std::map", "std::unique_ptr",
                               "std::shared_ptr", "int32_t",     "uint32_t", "size_t"};
    for (const auto &t : types) {
        m_rules.push_back(
            {.pattern = QRegularExpression("\\b" + t + "\\b"), .format = m_typeFormat});
    }

    // Numbers (hex/bin/oct/float)
    m_rules.push_back({.pattern = QRegularExpression(
                           R"(\b(0[xX][0-9A-Fa-f]+|0[bB][01]+|0[0-7]*|[1-9][0-9]*(\.[0-9]+)?)\b)"),
                       .format = m_numberFormat});

    // Functions (name(...))
    m_rules.push_back({.pattern = QRegularExpression(R"(\b[A-Za-z_][A-Za-z0-9_]*(?=\s*\())"),
                       .format = m_functionFormat});

    // Uppercase types (class names) - keep here (will be overridden inside string later)
    m_rules.push_back(
        {.pattern = QRegularExpression(R"(\b[A-Z][A-Za-z0-9_]*\b)"), .format = m_typeFormat});

    // Char literals (single quotes) — treat like strings
    m_rules.push_back(
        {.pattern = QRegularExpression(R"('(?:\\.|[^'\\])*')"), .format = m_stringFormat});

    // --- STRING RULES: keep separate and apply LAST in highlightBlock() ---
    // 1) Normal C++ string: "..." with escapes (single-line or multi-line when required)
    m_stringRules.push_back(
        {.pattern = QRegularExpression(R"("(?:\\.|[^"\\])*")",
                                       QRegularExpression::DotMatchesEverythingOption),
         .format = m_stringFormat});

    // 2) Angle-bracket includes: <...>
    m_stringRules.push_back(
        {.pattern = QRegularExpression(R"(<[^>]*>)"), .format = m_stringFormat});

    // 3) Raw string literals: R"delim(... )delim"  (support delimiters up to 16 chars)
    // Using outer raw string literal R"RAW(... )RAW" so inner quotes are safe.
    m_stringRules.push_back(
        {.pattern = QRegularExpression(R"RAW(R"([A-Za-z0-9_]{0,16})\((?:.|\n)*?\)\1")RAW",
                                       QRegularExpression::DotMatchesEverythingOption |
                                           QRegularExpression::MultilineOption),
         .format = m_stringFormat});

    // --- Delimiters: {}, (), [] --- (Regex: \b không cần thiết vì đây là ký tự đơn)
    // Lưu ý: Tách riêng các ký tự để dễ dàng loại trừ hoặc thay đổi sau này
    const QStringList delimiters = {"\\{", "\\}", "\\(", "\\)", "\\[", "\\]"};
    for (const auto &delim : delimiters) {
        m_rules.push_back({.pattern = QRegularExpression(delim), .format = m_delimiterFormat});
    }
}

void CppHighlighter::highlightBlock(const QString &text) {
    // Default app text color (used to detect "unformatted" regions)
    const QColor defaultTextColor = qApp->palette().color(QPalette::Text);

    // --- 0) Apply string-like rules FIRST so they own internal content ---
    for (const auto &rule : m_stringRules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int start = static_cast<const int>(match.capturedStart());
            const int len = static_cast<const int>(match.capturedLength());
            if (len <= 0) { continue; }

            QTextCharFormat f = rule.format;
            if (f.foreground().color().isValid()) {
                f.setForeground(QBrush(f.foreground().color(), Qt::SolidPattern));
            }
            setFormat(start, len, f);
        }
    }

    // --- 1) Apply normal rules (keywords, types, numbers, functions, etc.)
    // But SKIP ranges that are already formatted (i.e. not using defaultTextColor)
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int start = static_cast<const int>(match.capturedStart());
            const int len = static_cast<const int>(match.capturedLength());
            if (len <= 0) { continue; }

            // check whether any char in this range is already formatted (i.e. not default)
            bool alreadyFormatted = false;
            for (int i = 0; i < len; ++i) {
                const QColor c = format(start + i).foreground().color();
                if (c.isValid() && c != defaultTextColor) {
                    alreadyFormatted = true;
                    break;
                }
            }
            if (alreadyFormatted) {
                continue; // don't override (preserve string/other)
            }

            QTextCharFormat f = rule.format;
            if (f.foreground().color().isValid()) {
                f.setForeground(QBrush(f.foreground().color(), Qt::SolidPattern));
            }
            setFormat(start, len, f);
        }
    }

    // --- 2) Comments (single-line) --- apply but DO NOT override strings
    {
        QRegularExpression singleLineComment(R"(//[^\n]*)");
        auto it = singleLineComment.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int start = static_cast<const int>(match.capturedStart());
            const int len = static_cast<const int>(match.capturedLength());
            if (len <= 0) { continue; }

            // if comment overlaps a string (i.e. existing color == string color) skip
            bool inString = false;
            for (int i = 0; i < len; ++i) {
                const QColor c = format(start + i).foreground().color();
                if (c.isValid() && c == m_stringFormat.foreground().color()) {
                    inString = true;
                    break;
                }
            }
            if (inString) { continue; }

            QTextCharFormat f = m_commentFormat;
            if (f.foreground().color().isValid()) {
                f.setForeground(QBrush(f.foreground().color(), Qt::SolidPattern));
            }
            setFormat(start, len, f);
        }
    }

    // --- 2b) Multi-line comments (unchanged logic) ---
    {
        QRegularExpression startRE(R"(/\*)");
        QRegularExpression endRE(R"(\*/)");
        setCurrentBlockState(0);
        int startIndex = 0;
        if (previousBlockState() != 1) { startIndex = static_cast<int>(text.indexOf(startRE)); }

        while (startIndex >= 0) {
            int endIndex = static_cast<int>(text.indexOf(endRE, startIndex));
            int commentLength{};
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = static_cast<int>(text.length() - startIndex);
            } else {
                commentLength = endIndex - startIndex + 2;
            }

            // check if this comment is inside a string (if so skip)
            bool inString = false;
            for (int i = 0; i < commentLength; ++i) {
                const QColor c = format(startIndex + i).foreground().color();
                if (c.isValid() && c == m_stringFormat.foreground().color()) {
                    inString = true;
                    break;
                }
            }
            if (!inString) {
                QTextCharFormat cf = m_commentFormat;
                if (cf.foreground().color().isValid()) {
                    cf.setForeground(QBrush(cf.foreground().color(), Qt::SolidPattern));
                }
                setFormat(startIndex, commentLength, cf);
            }

            startIndex = static_cast<int>(text.indexOf(startRE, startIndex + commentLength));
        }
    }

    // --- 3) Identifier fallback (names) - only when currently default or equals app default color
    {
        QRegularExpression identifierRegex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\b)");
        auto it = identifierRegex.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const int s = static_cast<const int>(m.capturedStart());
            const int l = static_cast<const int>(m.capturedLength());
            if (l <= 0) { continue; }

            // If any char already has non-default color, skip
            bool alreadyColored = false;
            for (int i = 0; i < l; ++i) {
                const QColor c = format(s + i).foreground().color();
                if (c.isValid() && c != defaultTextColor) {
                    alreadyColored = true;
                    break;
                }
            }
            if (alreadyColored) { continue; }

            QTextCharFormat idf = m_identifierFormat;
            if (idf.foreground().color().isValid()) {
                idf.setForeground(QBrush(idf.foreground().color(), Qt::SolidPattern));
            }
            setFormat(s, l, idf);
        }
    }
}

void CppHighlighter::stopGradualRehighlight() {
    if (m_gradualTimer != nullptr) {
        m_gradualTimer->stop();
        m_gradualTimer->deleteLater();
        m_gradualTimer = nullptr;
    }
    m_targetDoc.clear();
    m_currentBlockIndex = 0;
}

void CppHighlighter::rehighlightGradually(QTextDocument* doc, int batchSize, int intervalMs) {
    // stop any previous run
    stopGradualRehighlight();

    if (doc == nullptr) { return; }

    m_targetDoc = doc;
    m_batchSize = std::max(1, batchSize);
    m_currentBlockIndex = 0;

    // create timer as child of this (safe)
    m_gradualTimer = new QTimer(this);
    m_gradualTimer->setInterval(std::max(1, intervalMs));

    connect(m_gradualTimer, &QTimer::timeout, this, &CppHighlighter::onGradualTimerTimeout);

    // start
    m_gradualTimer->start();
}

void CppHighlighter::onGradualTimerTimeout() {
    // guard: if doc died, stop
    if (m_targetDoc.isNull()) {
        stopGradualRehighlight();
        return;
    }

    const int totalBlocks = m_targetDoc->blockCount();
    if (m_currentBlockIndex >= totalBlocks) {
        stopGradualRehighlight();
        return;
    }

    // Process up to batchSize blocks in this tick
    int processed = 0;
    while (processed < m_batchSize && m_currentBlockIndex < totalBlocks) {
        QTextBlock block = m_targetDoc->findBlockByNumber(m_currentBlockIndex);
        if (block.isValid()) {
            // rehighlightBlock expects a QTextBlock, but some Qt versions expect block as argument
            // Use protected API rehighlightBlock(QTextBlock) if available; else call
            // rehighlightBlock by index:
            rehighlightBlock(block);
        }
        ++m_currentBlockIndex;
        ++processed;
    }

    // if finished
    if (m_currentBlockIndex >= totalBlocks) { stopGradualRehighlight(); }
}
