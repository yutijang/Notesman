#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QPointer>
#include <QObject>

#include "cpphighlightertheme.hpp"

class QTimer;

class CppHighlighter final : public QSyntaxHighlighter {
        Q_OBJECT

    public:
        explicit CppHighlighter(QTextDocument* parent, const CppHighlighterTheme &theme);
        ~CppHighlighter() override;

        // Cho phép đổi theme lúc runtime
        void setTheme(const CppHighlighterTheme &theme);

        // rehighlight gradually (non-blocking)
        void rehighlightGradually(QTextDocument* doc,
                                  int batchSize = 10,  // NOLINT(readability-magic-numbers)
                                  int intervalMs = 5); // NOLINT(readability-magic-numbers)
        void stopGradualRehighlight();

    protected:
        void handleSingleLineComment(const QString &text);
        void handleMultilineComment(const QString &text);
        void handleIdentifierFallback(const QString &text, const QColor &defaultTextColor);
        void applyNormalRules(const QString &text, const QColor &defaultTextColor);
        void applyStringlikeRules(const QString &text);
        void highlightBlock(const QString &text) override;

    private:
        void initRules();
        void onGradualTimerTimeout();

        struct HighlightRule {
                QRegularExpression pattern;
                QTextCharFormat format;
        };

        QVector<HighlightRule> m_rules;
        QVector<HighlightRule> m_stringRules;
        QTextCharFormat m_commentFormat;
        QTextCharFormat m_stringFormat;
        QTextCharFormat m_flowKwFormat;
        QTextCharFormat m_builtinKwFormat;
        QTextCharFormat m_typeFormat;
        QTextCharFormat m_numberFormat;
        QTextCharFormat m_preprocessorFormat;
        QTextCharFormat m_functionFormat;
        QTextCharFormat m_identifierFormat;
        QTextCharFormat m_delimiterFormat;

        CppHighlighterTheme m_theme;

        // gradual rehighlight state
        QTimer* m_gradualTimer{};
        QPointer<QTextDocument> m_targetDoc; // doc rehighlighting
        int m_currentBlockIndex{};
        int m_batchSize{10};                 // NOLINT(readability-magic-numbers)
};
