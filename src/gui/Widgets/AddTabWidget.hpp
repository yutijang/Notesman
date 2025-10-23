#pragma once

#include <QWidget>
#include <QStringList>

class TagInput;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTextEdit;
class QPushButton;

class AddTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit AddTabWidget(QWidget* parent = nullptr);
        ~AddTabWidget() override = default;

        void retranslateUi();

        // Getter
        [[nodiscard]] int editorWidth() const noexcept;
        [[nodiscard]] QRadioButton* textRadio() const noexcept;
        [[nodiscard]] QRadioButton* fileRadio() const noexcept;
        [[nodiscard]] QLineEdit* filePathInput() const noexcept;
        [[nodiscard]] QLineEdit* titleInput() const noexcept;
        [[nodiscard]] QTextEdit* textEdit() const noexcept;
        [[nodiscard]] TagInput* tagInput() const noexcept;
        [[nodiscard]] QWidget* fileContainer() const noexcept;

    signals:
        void addNoteRequested(QString title, QString textContent, QString filePath,
                              QStringList tags, bool isTextMode);

    private slots:
        void onAddButtonClicked();
        void onClearButtonClicked();
        void onTextRadioToggled(bool checked);
        void onBrowseFile();
        void updateAddAndClearButtons();

    private: // NOLINT(readability-redundant-access-specifiers)
        void setupUi();
        void setupConnections();
        void clearFields();

        QLabel* m_titleLbl{};
        QLineEdit* m_titleInp{};
        QLabel* m_resTypeLbl{};
        QRadioButton* m_textRad{};
        QRadioButton* m_fileRad{};
        QWidget* m_fileContainer{};
        QLabel* m_filepathLbl{};
        QLineEdit* m_filepathInp{};
        QTextEdit* m_textEdt{};
        QPushButton* m_addBtn{};
        QPushButton* m_browseBtn{};
        QPushButton* m_clearBtn{};
        TagInput* m_tagInp{};

        int m_EditorWidth{};
};
