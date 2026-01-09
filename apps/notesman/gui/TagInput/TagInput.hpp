#pragma once

#include <QWidget>
#include <QStringList>
#include <QObject>
#include <QList>
#include <QString>

class QHBoxLayout;
class QLineEdit;
class QPushButton;

class TagInput final : public QWidget {
        Q_OBJECT

    public:
        explicit TagInput(QWidget* parent = nullptr);

        void addTag(const QString &tag);
        [[nodiscard]] QStringList getAllTags() const;

        void retranslateUi();

    signals:
        void tagAdded(const QString &tag);

    public slots:
        void clearTags();

    private slots:
        void onTextChanged(const QString &text);
        void onTagClicked();
        void onReturnPressed();

    private: // NOLINT(readability-redundant-access-specifiers)
        void createInput();

        QHBoxLayout* m_layout{};
        QLineEdit* m_input{};
        QList<QPushButton*> m_tags;
};
