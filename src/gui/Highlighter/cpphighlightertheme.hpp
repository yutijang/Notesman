#pragma once

#include <QColor>

struct CppHighlighterTheme {
        QColor type;
        QColor string;
        QColor comment;
        QColor number;
        QColor preprocessor;
        QColor function;
        QColor identifier;
        QColor flowKwFormat;
        QColor builtinKwFormat;
        QColor delimiter;
};

inline CppHighlighterTheme createLightTheme() {
    return {.type = QColor("#2B91AF"),            // type
            .string = QColor("#A31515"),          // string
            .comment = QColor("#008000"),         // comment
            .number = QColor("#098658"),          // number
            .preprocessor = QColor("#0000FF"),    // preprocessor
            .function = QColor("#795E26"),        // function
            .identifier = QColor("#001080"),      // identifier
            .flowKwFormat = QColor("#af00db"),    // flow keyword
            .builtinKwFormat = QColor("#0000FF"), // built-in keyword
            .delimiter = QColor("#FF4500")};
}

inline CppHighlighterTheme createDarkTheme() {
    return {.type = QColor("#4EC9B0"),            // type
            .string = QColor("#CE9178"),          // string
            .comment = QColor("#6A9955"),         // comment
            .number = QColor("#B5CEA8"),          // number
            .preprocessor = QColor("#C586C0"),    // preprocessor
            .function = QColor("#DCDCAA"),        // function
            .identifier = QColor("#9CDCFE"),      // identifier
            .flowKwFormat = QColor("#C586C0"),    // flow keyword
            .builtinKwFormat = QColor("#569CD6"), // built-in keyword
            .delimiter = QColor("#FFD700")};
}
