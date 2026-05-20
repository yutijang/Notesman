#pragma once

#include "gui/UiConstants.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QWidget>

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
class QButtonGroup;

class AddTabWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit AddTabWidget(QWidget* parent = nullptr);
    ~AddTabWidget() override = default;

    void retranslateUi();

    // Getter
    [[nodiscard]] PlainTextEdit* textEdit() const noexcept {
        return m_textEdt;
    }

    void showNotification(
        QString const& message,
        UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal) const;
    void resetAddTabInputs() const;

  Q_SIGNALS:
    void addNoteRequested(QString title,
                          QString textContent,
                          QString filePath,
                          QString url,
                          QStringList tags,
                          UiConst::AddResMode mode);
    void applySyntaxHighlighterRequest(bool checked);

  private:
    void setupUi();
    void setupConnections();
    void onToggleCodeHighlighter(bool checked);
    static QString buildResourceFileFilter();
    void onAddResTypeModeChanged(int id);

    void onAddButtonClicked();
    void onClearButtonClicked();
    void onBrowseFile();
    void updateAddAndClearButtons();

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
    QRadioButton* m_urlRad{};
    QWidget* m_fileContainer{};
    QLabel* m_filepathInpLbl{};
    QLineEdit* m_filepathInp{};
    QLabel* m_urlInpLbl{};
    QLineEdit* m_urlInp{};
    PlainTextEdit* m_textEdt{};
    QPushButton* m_addBtn{};
    QPushButton* m_browseBtn{};
    QPushButton* m_clearBtn{};
    TagInput* m_tagInp{};
    QLabel* m_notiLbl{};
    QLabel* m_notiFilepathLbl{};
    QWidget* m_textEditorContainer{};
    QCheckBox* m_toggleCodeHighlighterChkb{};
    QButtonGroup* m_addResTypeGroup{};
};
