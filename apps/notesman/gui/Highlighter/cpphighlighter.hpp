#pragma once

#include "gui/Highlighter/cpphighlightertheme.hpp"

#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>

class QTimer;

class CppHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT

  public:
    explicit CppHighlighter(QTextDocument* parent, CppHighlighterTheme const& theme);
    ~CppHighlighter() override;

    // Cho phép đổi theme lúc runtime
    void setTheme(CppHighlighterTheme const& theme);

    // rehighlight gradually (non-blocking)
    void rehighlightGradually(QTextDocument* doc,
                              int batchSize = 10,  // NOLINT(readability-magic-numbers)
                              int intervalMs = 5); // NOLINT(readability-magic-numbers)
    void stopGradualRehighlight();

  protected:
    void handleSingleLineComment(QString const& text);
    void handleMultilineComment(QString const& text);
    void handleIdentifierFallback(QString const& text, QColor const& defaultTextColor);
    void applyNormalRules(QString const& text, QColor const& defaultTextColor);
    void applyStringlikeRules(QString const& text);
    void highlightBlock(QString const& text) override;

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

    // gradual rehighlight state
    QTimer* m_gradualTimer{};
    QPointer<QTextDocument> m_targetDoc; // doc rehighlighting
    int m_currentBlockIndex{};
    int m_batchSize{10};                 // NOLINT(readability-magic-numbers)

    CppHighlighterTheme m_theme;
};
