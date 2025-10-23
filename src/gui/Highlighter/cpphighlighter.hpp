#pragma once
#include "cpphighlightertheme.hpp"

class QSyntaxHighlighter;
class QTextDocument;
class QRegularExpression;
class QTextCharFormat;
class QTimer;
class QString;

class CppHighlighter final : public QSyntaxHighlighter {
        Q_OBJECT
    public:
        explicit CppHighlighter(QTextDocument* parent, const CppHighlighterTheme &theme);
        ~CppHighlighter();

        // Cho phép đổi theme lúc runtime
        void setTheme(const CppHighlighterTheme &theme);

        // new API: rehighlight gradually (non-blocking)
        void rehighlightGradually(QTextDocument* doc,
                                  int batchSize = 10,  // NOLINT(readability-magic-numbers)
                                  int intervalMs = 5); // NOLINT(readability-magic-numbers)
        void stopGradualRehighlight();
    private slots:
        void onGradualTimerTimeout();

    protected:
        void highlightBlock(const QString &text) override;

    private:
        void initRules();

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
        QTimer* m_gradualTimer{nullptr};
        QPointer<QTextDocument> m_targetDoc; // doc we're rehighlighting
        int m_currentBlockIndex{0};
        int m_batchSize{10};                 // NOLINT(readability-magic-numbers)
};
