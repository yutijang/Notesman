#include "gui/Widgets/AddTabWidget.hpp"

#include "core/model/model.hpp"
#include "gui/TagInput/TagInput.hpp"
#include "gui/UiConstants.hpp"
#include "helper/DialogUtils.hpp"
#include "helper/PlainTextEdit.hpp"
#include "helper/SettingsManager.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <Qt>

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

    onAddResTypeModeChanged(static_cast<int>(UiConst::AddResMode::Text));

    updateAddAndClearButtons();
}

void AddTabWidget::setupConnections() {
    QObject::connect(m_addBtn, &QPushButton::clicked, this, &AddTabWidget::onAddButtonClicked);
    QObject::connect(m_clearBtn, &QPushButton::clicked, this, &AddTabWidget::onClearButtonClicked);

    QObject::connect(
        m_addResTypeGroup, &QButtonGroup::idClicked, this, &AddTabWidget::onAddResTypeModeChanged);

    QObject::connect(m_toggleCodeHighlighterChkb,
                     &QCheckBox::toggled,
                     this,
                     &AddTabWidget::onToggleCodeHighlighter);

    QObject::connect(
        m_titleInp, &QLineEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);
    QObject::connect(
        m_textEdt, &PlainTextEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);
    QObject::connect(
        m_filepathInp, &QLineEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);
    QObject::connect(
        m_urlInp, &QLineEdit::textChanged, this, &AddTabWidget::updateAddAndClearButtons);

    QObject::connect(m_browseBtn, &QPushButton::clicked, this, &AddTabWidget::onBrowseFile);
}

void AddTabWidget::onAddButtonClicked() {
    QString const title = m_titleInp->text().trimmed();
    QStringList const tags = m_tagInp->getAllTags();

    QString text;
    QString filePath;
    QString url;

    auto const mode = static_cast<UiConst::AddResMode>(m_addResTypeGroup->checkedId());
    switch (mode) {
        case UiConst::AddResMode::Text: {
            text = m_textEdt->toPlainText();
            break;
        }
        case UiConst::AddResMode::File: {
            filePath = m_filepathInp->text().trimmed();
            break;
        }
        case UiConst::AddResMode::Url: {
            url = m_urlInp->text().trimmed();
            break;
        }
    }

    Q_EMIT addNoteRequested(title, text, filePath, url, tags, mode);
}

void AddTabWidget::onClearButtonClicked() {
    auto const reply = DialogUtils::showQuestion(
        this, tr("Caution"), tr("Would you like to clear content in all data field?"));

    if (reply == QMessageBox::Yes) {
        resetAddTabInputs();
    }
}

void AddTabWidget::onBrowseFile() {
    auto& qSettings = SettingsManager::instance();

    QString const kDefaultDir = qSettings.get("addTab/lastBrowseDir", QDir::homePath()).toString();

    QString const fileFilter = buildResourceFileFilter();
    QString const filePath =
        QFileDialog::getOpenFileName(this, tr("Select Resource File"), kDefaultDir, fileFilter);

    if (filePath.isEmpty()) {
        return;
    }

    m_filepathInp->setText(QDir::toNativeSeparators(filePath));

    QFileInfo fileInfo(filePath);

    if (m_titleInp->text().trimmed().isEmpty()) {
        m_titleInp->setText(fileInfo.completeBaseName());
    }

    if (fileInfo.exists()) {
        qSettings.set("addTab/lastBrowseDir", fileInfo.absoluteDir().path());
    }

    auto const typeOpt = resourceTypeFromFile(filePath.toStdString());
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
    bool const hasTitle = !m_titleInp->text().trimmed().isEmpty();

    auto const mode = static_cast<UiConst::AddResMode>(m_addResTypeGroup->checkedId());
    switch (mode) {
        case UiConst::AddResMode::Text: {
            bool const hasText = !m_textEdt->toPlainText().trimmed().isEmpty();

            m_addBtn->setEnabled(hasTitle && hasText);
            m_clearBtn->setEnabled(hasTitle || hasText);
            break;
        }

        case UiConst::AddResMode::File: {
            bool const hasFilePath = !m_filepathInp->text().trimmed().isEmpty();

            m_addBtn->setEnabled(hasTitle && hasFilePath);
            m_clearBtn->setEnabled(hasTitle || hasFilePath);
            break;
        }

        case UiConst::AddResMode::Url: {
            bool const hasUrl = !m_urlInp->text().trimmed().isEmpty();

            m_addBtn->setEnabled(hasTitle && hasUrl);
            m_clearBtn->setEnabled(hasTitle || hasUrl);
            break;
        }
    }
}

void AddTabWidget::retranslateUi() {
    m_titleLbl->setText(tr("Title"));
    m_titleInp->setPlaceholderText(tr("Enter title for resource..."));
    m_resTypeLbl->setText(tr("Resource type:"));
    m_textRad->setText(tr("Text"));
    m_filepathInpLbl->setText(tr("File path"));
    m_filepathInp->setPlaceholderText(tr("Enter file path..."));

    m_urlInp->setPlaceholderText(tr("Enter url of web page..."));

    if (m_tagInp != nullptr) {
        m_tagInp->retranslateUi();
    }

    m_toggleCodeHighlighterChkb->setText(tr("Toggle syntax highlight"));

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

    m_addResTypeGroup = new QButtonGroup(this);
    m_textRad = new QRadioButton(tr("Text"));
    m_fileRad = new QRadioButton("File");
    m_urlRad = new QRadioButton("Url");
    m_textRad->setChecked(true);

    m_addResTypeGroup->addButton(m_textRad, static_cast<int>(UiConst::AddResMode::Text));
    m_addResTypeGroup->addButton(m_fileRad, static_cast<int>(UiConst::AddResMode::File));
    m_addResTypeGroup->addButton(m_urlRad, static_cast<int>(UiConst::AddResMode::Url));

    resTypeLayout->addWidget(m_resTypeLbl);
    resTypeLayout->addWidget(m_textRad);
    resTypeLayout->addWidget(m_fileRad);
    resTypeLayout->addWidget(m_urlRad);

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

    m_filepathInpLbl = new QLabel(tr("File path"));
    m_filepathInp = new QLineEdit();
    m_filepathInp->setPlaceholderText(tr("Enter file path..."));

    m_urlInpLbl = new QLabel("Url");
    m_urlInp = new QLineEdit();
    m_urlInp->setPlaceholderText(tr("Enter url of web page..."));

    m_browseBtn = new QPushButton("...");
    m_browseBtn->setMaximumWidth(UiConst::BUTTON_NEXT_INPUT_WIDTH);
    m_browseBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_filepathInp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_browseBtn->setProperty("targetEdit", QVariant::fromValue(m_filepathInp));

    auto* fileHBoxLayout = new QHBoxLayout(); // Sub-HBox cho input + button
    fileHBoxLayout->setContentsMargins(0, 0, 0, 0);
    fileHBoxLayout->addWidget(m_filepathInp);
    fileHBoxLayout->addWidget(m_browseBtn);

    fileHBoxLayout->addWidget(m_urlInp);

    m_notiFilepathLbl = new QLabel("");
    m_notiFilepathLbl->setVisible(false);
    m_notiFilepathLbl->setObjectName("checkFilePath");

    QFont currentFont = m_notiFilepathLbl->font();
    currentFont.setPointSize(9); // NOLINT(readability-magic-numbers)
    m_notiFilepathLbl->setFont(currentFont);

    m_notiFilepathLbl->setContentsMargins(0, 0, 0, 0);

    fileVBoxLayout->addWidget(m_filepathInpLbl);
    fileVBoxLayout->addWidget(m_urlInpLbl);
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

    m_toggleCodeHighlighterChkb = new QCheckBox(tr("Toggle syntax highlight"), toolbar);
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

void AddTabWidget::showNotification(QString const& message,
                                    UiConst::SettingsTabNotiLevel notiType) const {
    m_notiLbl->setText(message);
    m_notiLbl->setVisible(true);

    if (notiType != UiConst::SettingsTabNotiLevel::Normal) {
        QString notiTextColor{};
        switch (notiType) {
            case UiConst::SettingsTabNotiLevel::Good: {
                notiTextColor = "#2ECC71";
                break;
            }
            case UiConst::SettingsTabNotiLevel::Normal : break;
            case UiConst::SettingsTabNotiLevel::Caution: {
                notiTextColor = "#D97706";
                break;
            }
            case UiConst::SettingsTabNotiLevel::Warning: {
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
    m_urlInp->clear();

    m_titleInp->setFocus();
}

void AddTabWidget::onToggleCodeHighlighter(bool checked) {
    Q_EMIT applySyntaxHighlighterRequest(checked);
}

QString AddTabWidget::buildResourceFileFilter() {
    QMap<ResourceType, QStringList> groups;
    QStringList allExtensions;

    for (auto const& [ext, type] : K_EXT_MAP) {
        QString pattern = "*." + QString::fromUtf8(ext.data(), static_cast<int>(ext.size()));
        groups[type] << pattern;
        allExtensions << pattern;
    }

    QStringList filters;

    if (!allExtensions.isEmpty()) {
        filters << tr("All Supported Files (%1)").arg(allExtensions.join(' '));
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        QString label;
        switch (it.key()) {
            case ResourceType::PlainText: label = tr("Text Files"); break;
            case ResourceType::CCppCode : label = tr("C/C++ Source"); break;
            case ResourceType::HtmlDoc  : label = tr("HTML Documents"); break;
            case ResourceType::PdfDoc   : label = tr("PDF Documents"); break;
            case ResourceType::EpubDoc  : label = tr("Epub Books"); break;
            default                     : label = tr("Other Files"); break;
        }
        filters << QString("%1 (%2)").arg(label, it.value().join(' '));
    }

    filters << tr("All Files (*)");

    return filters.join(";;");
}

void AddTabWidget::onAddResTypeModeChanged(int id) {
    auto const mode = static_cast<UiConst::AddResMode>(id);

    switch (mode) {
        case UiConst::AddResMode::Text: {
            m_fileContainer->hide();
            m_textEditorContainer->show();

            break;
        }
        case UiConst::AddResMode::File: {
            m_fileContainer->show();
            m_textEditorContainer->hide();

            m_filepathInpLbl->show();
            m_filepathInp->show();
            m_browseBtn->show();

            m_urlInpLbl->hide();
            m_urlInp->hide();

            break;
        }
        case UiConst::AddResMode::Url: {
            m_fileContainer->show();
            m_textEditorContainer->hide();

            m_urlInpLbl->show();
            m_urlInp->show();

            m_filepathInpLbl->hide();
            m_filepathInp->hide();
            m_browseBtn->hide();

            break;
        }
    }
}
