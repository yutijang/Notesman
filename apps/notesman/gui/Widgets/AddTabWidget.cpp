#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTextEdit>
#include <QFont>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include <QCheckBox>
#include <QObject>
#include <Qt>
#include <QStringList>
#include <QFileInfo>

#include "PlainTextEdit.hpp"
#include "TagInput.hpp"
#include "AddTabWidget.hpp"
#include "UiConstants.hpp"
#include "model.hpp"
#include "DialogUtils.hpp"
#include "SettingsManager.hpp"

AddTabWidget::AddTabWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    setupConnections();
}

void AddTabWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);

    auto* controlPanel = new QWidget(this);
    auto* controlLayout = new QVBoxLayout(controlPanel);

    m_notiLbl = new QLabel();
    m_notiLbl->setAlignment(Qt::AlignCenter);
    m_notiLbl->setVisible(false);

    // Add to layout
    controlLayout->addLayout(setupTitleGroup());
    controlLayout->addWidget(setupResouceGroup(), 0, Qt::AlignHCenter);
    controlLayout->addLayout(setupTagGroup());
    controlLayout->addWidget(setupFilePathGroup());
    controlLayout->addStretch(1);
    controlLayout->addWidget(m_notiLbl);
    controlLayout->addSpacing(15); // NOLINT(readability-magic-numbers)
    controlLayout->addLayout(setupButtonGroup());

    mainLayout->addWidget(controlPanel, 4);
    mainLayout->addWidget(setupTextEditorGroup(), 6); // NOLINT(readability-magic-numbers)

    onTextRadioToggled(true);
    updateAddAndClearButtons();
}

void AddTabWidget::setupConnections() {
    QObject::connect(m_addBtn, &QPushButton::clicked, this, &AddTabWidget::onAddButtonClicked);
    QObject::connect(m_clearBtn, &QPushButton::clicked, this, &AddTabWidget::onClearButtonClicked);
    QObject::connect(m_textRad, &QRadioButton::toggled, this, &AddTabWidget::onTextRadioToggled);
    QObject::connect(m_toggleCodeHighlighterChkb, &QCheckBox::toggled, this,
                     &AddTabWidget::onToggleCodeHighlighter);

    QObject::connect(m_titleInp, &QLineEdit::textChanged, this,
                     &AddTabWidget::updateAddAndClearButtons);
    QObject::connect(m_textEdt, &PlainTextEdit::textChanged, this,
                     &AddTabWidget::updateAddAndClearButtons);
    QObject::connect(m_filepathInp, &QLineEdit::textChanged, this,
                     &AddTabWidget::updateAddAndClearButtons);

    QObject::connect(m_browseBtn, &QPushButton::clicked, this, &AddTabWidget::onBrowseFile);
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
    const auto reply = DialogUtils::showQuestion(
        this, tr("Caution"), tr("Would you like to clear content in all data field?"));

    if (reply == QMessageBox::Yes) { clearFields(); }
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
        m_textEditorContainer->show();
    } else {
        m_fileContainer->show();
        m_textEditorContainer->hide();
    }
}

void AddTabWidget::onBrowseFile() {
    auto &settings = SettingsManager::instance();

    const QString kDefaultDir = settings.get("addTab/lastBrowseDir", QDir::homePath()).toString();

    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Resource File"), kDefaultDir,
        tr("All Files (*);;Text Files (*.txt *.md);;C++ Source (*.cpp *.h)"));

    if (filePath.isEmpty()) { return; }

    m_filepathInp->setText(QDir::toNativeSeparators(filePath));

    QFileInfo fileInfo(filePath);

    m_titleInp->setText(fileInfo.completeBaseName());

    if (fileInfo.exists()) { settings.set("addTab/lastBrowseDir", fileInfo.absoluteDir().path()); }

    const auto typeOpt = resourceTypeFromFile(filePath.toStdString());
    if (!typeOpt.has_value()) {
        m_notiFilepathLbl->setText(tr("File extension not support!"));
        m_notiFilepathLbl->setVisible(true);

        QTimer::singleShot(UiConst::NOTI_TIMEOUT5, this, [this]() {
            m_notiFilepathLbl->clear();
            m_notiFilepathLbl->setVisible(false);
        });
    }
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

// Title Group
QVBoxLayout* AddTabWidget::setupTitleGroup() {
    auto* titleLayout = new QVBoxLayout();

    m_titleLbl = new QLabel(tr("Title"));
    m_titleInp = new QLineEdit();
    m_titleInp->setPlaceholderText(tr("Enter title for resource..."));

    titleLayout->addWidget(m_titleLbl);
    titleLayout->addWidget(m_titleInp);

    return titleLayout;
}

// Resource type
QWidget* AddTabWidget::setupResouceGroup() {
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

    return resTypeContainer;
}

// Tag Group
QVBoxLayout* AddTabWidget::setupTagGroup() {
    auto* tagLayout = new QVBoxLayout();

    auto* resTagLabel = new QLabel("Tag");
    m_tagInp = new TagInput();

    tagLayout->addWidget(resTagLabel);
    tagLayout->addWidget(m_tagInp);

    return tagLayout;
}

// File input section
QWidget* AddTabWidget::setupFilePathGroup() {
    m_fileContainer = new QWidget();
    auto* fileVBoxLayout = new QVBoxLayout(m_fileContainer);
    fileVBoxLayout->setContentsMargins(0, 0, 0, 0);
    fileVBoxLayout->setSpacing(3); // Khoảng cách nhỏ giữa label và input

    m_filepathLbl = new QLabel(tr("File path"));
    m_filepathInp = new QLineEdit();
    m_filepathInp->setPlaceholderText(tr("Enter file path..."));

    m_browseBtn = new QPushButton("...");
    m_browseBtn->setMaximumWidth(UiConst::BUTTON_NEXT_INPUT_WIDTH);
    m_browseBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_filepathInp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_browseBtn->setProperty("targetEdit", QVariant::fromValue(m_filepathInp));

    auto* fileHBoxLayout = new QHBoxLayout(); // Sub-HBox cho input + button
    fileHBoxLayout->setContentsMargins(0, 0, 0, 0);
    fileHBoxLayout->addWidget(m_filepathInp);
    fileHBoxLayout->addWidget(m_browseBtn);

    m_notiFilepathLbl = new QLabel("");
    m_notiFilepathLbl->setVisible(false);
    m_notiFilepathLbl->setObjectName("checkFilePath");

    QFont currentFont = m_notiFilepathLbl->font();
    currentFont.setPointSize(9); // NOLINT(readability-magic-numbers)
    m_notiFilepathLbl->setFont(currentFont);

    m_notiFilepathLbl->setContentsMargins(0, 0, 0, 0);

    fileVBoxLayout->addWidget(m_filepathLbl);
    fileVBoxLayout->addLayout(fileHBoxLayout); // Add sub-layout
    fileVBoxLayout->addWidget(m_notiFilepathLbl);

    return m_fileContainer;
}

// Text editor
QWidget* AddTabWidget::setupTextEditorGroup() {
    m_textEditorContainer = new QWidget(this);
    auto* textLayout = new QVBoxLayout(m_textEditorContainer);
    textLayout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QWidget(m_textEditorContainer);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 3);

    m_toggleCodeHighlighterChkb = new QCheckBox("Toggle syntax highlight", toolbar);
    m_toggleCodeHighlighterChkb->setChecked(true);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(m_toggleCodeHighlighterChkb);

    m_textEdt = new PlainTextEdit(this);
    m_textEdt->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard |
                                       Qt::TextEditable);
    m_textEdt->setFont(QFont("JetBrains Mono", UiConst::FONT_SIZE));

    textLayout->addWidget(toolbar);
    textLayout->addWidget(m_textEdt, 1);

    return m_textEditorContainer;
}

// Buttons
QHBoxLayout* AddTabWidget::setupButtonGroup() {
    auto* buttonLayout = new QHBoxLayout();

    m_addBtn = new QPushButton(tr("Add"));
    m_addBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_addBtn->setIcon(QIcon(":/icons/add.ico"));

    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_clearBtn->setIcon(QIcon(":/icons/clear.ico"));

    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_addBtn);
    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addStretch(1);
    buttonLayout->setSpacing(10); // NOLINT(readability-magic-numbers)

    return buttonLayout;
}

void AddTabWidget::showNotification(const QString &message,
                                    UiConst::SettingsTabNotiLevel notiType) const {
    m_notiLbl->setText(message);
    m_notiLbl->setVisible(true);

    if (notiType != UiConst::SettingsTabNotiLevel::normal) {
        QString notiTextColor{};
        switch (notiType) {
            case UiConst::SettingsTabNotiLevel::good: {
                notiTextColor = "#2ECC71";
                break;
            }
            case UiConst::SettingsTabNotiLevel::normal : break;
            case UiConst::SettingsTabNotiLevel::caution: {
                notiTextColor = "#D97706";
                break;
            }
            case UiConst::SettingsTabNotiLevel::warning: {
                notiTextColor = "#E74C3C";
                break;
            }
            default: break; // NOLINT (-Wcovered-switch-default)
        }

        QString objName = m_notiLbl->objectName();
        if (objName.isEmpty()) {
            objName = "addTabNotiLbl";
            m_notiLbl->setObjectName(objName);
        }
        m_notiLbl->setStyleSheet(QString("#%1 { color: %2; }").arg(objName).arg(notiTextColor));
    }

    QTimer::singleShot(UiConst::NOTI_TIMEOUT, this, [this]() {
        m_notiLbl->clear();
        m_notiLbl->setVisible(false);
    });
}

void AddTabWidget::resetAddTabInputs() const {
    m_titleInp->clear();
    m_tagInp->clearTags();
    m_textEdt->clear();
    m_filepathInp->clear();

    m_titleInp->setFocus();
}

void AddTabWidget::onToggleCodeHighlighter(bool checked) {
    emit applySyntaxHighlighterRequest(checked);
}
