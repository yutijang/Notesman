#pragma once

#include <QWidget>
#include <QStringList>

class TagInput;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTextEdit;
class QPushButton;
class PlainTextEdit;
class QVBoxLayout;
class QHBoxLayout;
class QCheckBox;

class AddTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit AddTabWidget(QWidget* parent = nullptr);
        ~AddTabWidget() override = default;

        void retranslateUi();

        // Getter
        [[nodiscard]] PlainTextEdit* textEdit() const noexcept { return m_textEdt; }

    signals:
        void addNoteRequested(QString title, QString textContent, QString filePath,
                              QStringList tags, bool isTextMode);

    public slots:
        void showNotification(const QString &message) const;
        void resetAddTabInputs() const;

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

        QVBoxLayout* setupTitleGroup();
        QWidget* setupResouceGroup();
        QVBoxLayout* setupTagGroup();
        QWidget* setupFilePathGroup();
        QWidget* setupTextEditorGroup();
        QHBoxLayout* setupButtonGroup();

        QLabel* m_titleLbl{};
        QLineEdit* m_titleInp{};
        QLabel* m_resTypeLbl{};
        QRadioButton* m_textRad{};
        QRadioButton* m_fileRad{};
        QWidget* m_fileContainer{};
        QLabel* m_filepathLbl{};
        QLineEdit* m_filepathInp{};
        PlainTextEdit* m_textEdt{};
        QPushButton* m_addBtn{};
        QPushButton* m_browseBtn{};
        QPushButton* m_clearBtn{};
        TagInput* m_tagInp{};
        QLabel* m_notiLbl{};
        QLabel* m_notiFilepathLbl{};
        QWidget* m_textEditorContainer{};
        QCheckBox* m_toggleCodeHighlighterChkb{};
};
