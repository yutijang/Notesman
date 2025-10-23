#pragma once

class QWidget;
class QString;
class QHBoxLayout;
class QLineEdit;
class QPushButton;

class TagInput : public QWidget {
        Q_OBJECT
    public:
        explicit TagInput(QWidget* parent = nullptr);
        void addTag(const QString &tag);
        [[nodiscard]] QStringList getAllTags() const;

        void retranslateUi();

    public slots: // NOLINT(readability-redundant-access-specifiers)
        void clearTags();

    signals:
        void tagAdded(const QString &tag);

    private slots:
        void onTextChanged(const QString &text);
        void onTagClicked(); // Để remove tag
        void onReturnPressed();

    private:                 // NOLINT(readability-redundant-access-specifiers)
        void createInput();
        QHBoxLayout* m_layout{};
        QLineEdit* m_input{};
        QList<QPushButton*> m_tags; // Hoặc QPushButton cho clickable
};
