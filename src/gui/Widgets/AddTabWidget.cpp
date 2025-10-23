#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTextEdit>
#include <QFont>
#include "TagInput.hpp"
#include "AddTabWidget.hpp"

namespace {
    constexpr int BUTTON_WIDTH{100};
} // namespace

AddTabWidget::AddTabWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    setupConnections();
}

void AddTabWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);

    auto* controlPanel = new QWidget(this);
    auto* controlLayout = new QVBoxLayout(controlPanel);

    // Title
    m_titleLbl = new QLabel(tr("Title"));
    m_titleInp = new QLineEdit();
    m_titleInp->setPlaceholderText(tr("Enter title for resource..."));

    // Resource type
    auto* resTypeContainer = new QWidget();
    auto* resTypeLayout = new QHBoxLayout(resTypeContainer);
    resTypeLayout->setContentsMargins(0, 5, 0, 5); // NOLINT(readability-magic-numbers)
    resTypeLayout->setSpacing(20);                 // NOLINT(readability-magic-numbers)
    m_resTypeLbl = new QLabel(tr("Resource type:"));

    auto* resTypeGroup = new QButtonGroup(this);
    m_textRad = new QRadioButton(tr("Text"));
    m_fileRad = new QRadioButton("File");
    m_textRad->setChecked(true);

    resTypeGroup->addButton(m_textRad);
    resTypeGroup->addButton(m_fileRad);

    resTypeLayout->addWidget(m_resTypeLbl);
    resTypeLayout->addWidget(m_textRad);
    resTypeLayout->addWidget(m_fileRad);

    // Tag input
    auto* resTagLabel = new QLabel("Tag");
    m_tagInp = new TagInput(controlPanel);

    // File input section
    m_fileContainer = new QWidget(controlPanel);
    auto* fileVBoxLayout = new QVBoxLayout(m_fileContainer);
    fileVBoxLayout->setContentsMargins(0, 0, 0, 0);
    fileVBoxLayout->setSpacing(3); // Khoảng cách nhỏ giữa label và input

    m_filepathLbl = new QLabel(tr("File path"));
    m_filepathInp = new QLineEdit();
    m_filepathInp->setPlaceholderText(tr("Enter file path..."));
    m_filepathLbl->setBuddy(m_filepathInp);   // Best practice: Click label → focus input

    m_browseBtn = new QPushButton("...");
    m_browseBtn->setMaximumWidth(40);         // NOLINT(readability-magic-numbers)
    m_browseBtn->setProperty("targetEdit", QVariant::fromValue(m_filepathInp));

    auto* fileHBoxLayout = new QHBoxLayout(); // Sub-HBox cho input + button
    fileHBoxLayout->setContentsMargins(0, 0, 0, 0);
    fileHBoxLayout->addWidget(m_filepathInp);
    fileHBoxLayout->addWidget(m_browseBtn);

    fileVBoxLayout->addWidget(m_filepathLbl);
    fileVBoxLayout->addLayout(fileHBoxLayout); // Add sub-layout

    // Text editor
    m_textEdt = new QTextEdit();
    m_textEdt->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard |
                                       Qt::TextEditable);
    m_textEdt->setFont(QFont("JetBrains Mono", 11)); // NOLINT(readability-magic-numbers)

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    m_addBtn = new QPushButton(tr("Add"));
    m_addBtn->setMinimumWidth(BUTTON_WIDTH);
    m_addBtn->setIcon(QIcon(":/icons/add.ico"));

    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setMinimumWidth(BUTTON_WIDTH);
    m_clearBtn->setIcon(QIcon(":/icons/clear.ico"));

    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_addBtn);
    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addStretch(1);
    buttonLayout->setSpacing(10); // NOLINT(readability-magic-numbers)

    // Add to layout
    controlLayout->addWidget(m_titleLbl);
    controlLayout->addWidget(m_titleInp);
    controlLayout->addWidget(resTypeContainer, 0, Qt::AlignHCenter);
    controlLayout->addWidget(resTagLabel);
    controlLayout->addWidget(m_tagInp);
    controlLayout->addWidget(m_fileContainer);

    controlLayout->addStretch(1);

    controlLayout->addLayout(buttonLayout);

    mainLayout->addWidget(controlPanel, 4);
    mainLayout->addWidget(m_textEdt, 6); // NOLINT(readability-magic-numbers)
    m_EditorWidth = m_textEdt->width();

    onTextRadioToggled(true);
    updateAddAndClearButtons();
}

void AddTabWidget::setupConnections() {
    connect(m_addBtn, &QPushButton::clicked, this, &AddTabWidget::onAddButtonClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &AddTabWidget::onClearButtonClicked);
    connect(m_textRad, &QRadioButton::toggled, this, &AddTabWidget::onTextRadioToggled);
    connect(m_titleInp, &QLineEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);
    connect(m_textEdt, &QTextEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);
    connect(m_filepathInp, &QLineEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);

    connect(m_browseBtn, &QPushButton::clicked, this, &AddTabWidget::onBrowseFile);
    // connect(m_browseButton, &QPushButton::clicked, [this] { pickupFile(); });
}

void AddTabWidget::onAddButtonClicked() {
    const bool isTextMode = m_textRad->isChecked();
    const QString title = m_titleInp->text().trimmed();
    const QString filePath = m_filepathInp->text().trimmed();
    const QString text = m_textEdt->toPlainText();
    const QStringList tags = m_tagInp->getAllTags();

    emit addNoteRequested(title, text, filePath, tags, isTextMode);
}

void AddTabWidget::onClearButtonClicked() {
    const auto result = QMessageBox::question(
        this, tr("Caution"), tr("Would you like to clear content in all data field?"),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) { clearFields(); }
}

void AddTabWidget::clearFields() {
    m_titleInp->clear();
    m_textEdt->clear();
    m_filepathInp->clear();
    m_tagInp->clearTags();
}

void AddTabWidget::onTextRadioToggled(bool checked) {
    if (checked) {
        m_fileContainer->hide();
        m_textEdt->show();
    } else {
        m_fileContainer->show();
        m_textEdt->hide();
    }
}

void AddTabWidget::onBrowseFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Resource File"), QDir::homePath(),
        tr("All Files (*);;Text Files (*.txt *.md);;C++ Source (*.cpp *.h)"));

    if (!filePath.isEmpty()) { m_filepathInp->setText(QDir::toNativeSeparators(filePath)); }
}

// Logic enable/disable when m_titleInp, m_textEdt, m_filepathInp has content
void AddTabWidget::updateAddAndClearButtons() {
    const bool isTextMode = m_textRad->isChecked();

    if (isTextMode) {
        const bool hasTitle = !m_titleInp->text().trimmed().isEmpty();
        const bool hasText = !m_textEdt->toPlainText().trimmed().isEmpty();
        m_addBtn->setEnabled(hasTitle && hasText);
        m_clearBtn->setEnabled(hasTitle || hasText);
    } else {
        const bool hasTitle = !m_titleInp->text().trimmed().isEmpty();
        const bool hasFilePath = !m_filepathInp->text().trimmed().isEmpty();
        m_addBtn->setEnabled(hasTitle && hasFilePath);
        m_clearBtn->setEnabled(hasTitle || hasFilePath);
    }
}

void AddTabWidget::retranslateUi() {
    m_titleLbl->setText(tr("Title"));
    m_titleInp->setPlaceholderText(tr("Enter title for resource..."));
    m_resTypeLbl->setText(tr("Resource type:"));
    m_textRad->setText(tr("Text"));
    m_filepathLbl->setText(tr("File path"));
    m_filepathInp->setPlaceholderText(tr("Enter file path..."));

    if (m_tagInp != nullptr) { m_tagInp->retranslateUi(); }

    m_addBtn->setText(tr("Add"));
    m_clearBtn->setText(tr("Clear"));
}

int AddTabWidget::editorWidth() const noexcept {
    return m_EditorWidth;
}

QRadioButton* AddTabWidget::textRadio() const noexcept {
    return m_textRad;
}

QRadioButton* AddTabWidget::fileRadio() const noexcept {
    return m_fileRad;
}

QLineEdit* AddTabWidget::filePathInput() const noexcept {
    return m_filepathInp;
}

QLineEdit* AddTabWidget::titleInput() const noexcept {
    return m_titleInp;
}

QTextEdit* AddTabWidget::textEdit() const noexcept {
    return m_textEdt;
}

TagInput* AddTabWidget::tagInput() const noexcept {
    return m_tagInp;
}

QWidget* AddTabWidget::fileContainer() const noexcept {
    return m_fileContainer;
}