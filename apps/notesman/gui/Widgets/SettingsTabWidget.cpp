#include "SettingsTabWidget.hpp"

#include "DialogUtils.hpp"
#include "Logger.hpp"
#include "SettingsData.hpp"
#include "SettingsManager.hpp"
#include "UiConstants.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <Qt>
#include <filesystem>

namespace {
    constexpr int COUNTDOWN{60};
} // namespace

SettingsTabWidget::SettingsTabWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    setupConnections();
}

void SettingsTabWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);

    auto* contentWidget = new QWidget();
    auto* contentLayout = new QVBoxLayout(contentWidget);

    // Nhãn thông báo cập nhật Settings
    m_notiSettingsChangedLbl = new QLabel();
    m_notiSettingsChangedLbl->setAlignment(Qt::AlignCenter);
    m_notiSettingsChangedLbl->setVisible(false);

    contentLayout->addLayout(setupLanguageGroup());
    contentLayout->addLayout(setupThemeGroup());
    contentLayout->addLayout(setupResourceDirGroup());
    contentLayout->addLayout(setupResourceManagerTypeGroup());
    contentLayout->addWidget(setupFileAssociation());
    contentLayout->addWidget(setupCleanupGroup());
    contentLayout->addWidget(new QWidget);
    contentLayout->addLayout(setupAccountLinkGroup());
    contentLayout->addStretch(1);
    contentLayout->addWidget(m_notiSettingsChangedLbl);
    contentLayout->addLayout(setupButtonGroup());

    contentLayout->setSpacing(20); // NOLINT(readability-magic-numbers)

    // Bắt đầu xếp các widget và layout theo thứ tự mong muốn
    mainLayout->addStretch(1);
    mainLayout->addWidget(contentWidget, 3);
    mainLayout->addStretch(1);

    this->layout()->activate();
    this->setMinimumWidth(this->sizeHint().width());
}

void SettingsTabWidget::setupConnections() {
    // Kết nối các nút để phát tín hiệu cho MainWindow
    QObject::connect(m_applyBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onApplyBtnClicked);
    QObject::connect(m_defaultBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onDefaultBtnClicked);
    QObject::connect(m_resDirBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onBrowseBtnClicked);

    QObject::connect(m_uploadDBBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onUploadButtonClicked);
    QObject::connect(m_downloadDBBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onDownloadButtonClicked);
    QObject::connect(m_checkRemoteDBInfoBtn, &QPushButton::clicked, [this] {
        m_checkRemoteDBInfoBtn->setEnabled(false);
        Q_EMIT requestDBInfo();
    });

    QObject::connect(m_linkGDBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onLinkBtnClicked);

    QObject::connect(m_cancelLoginBtn, &QPushButton::clicked, this, [this]() {
        m_countdownTimer.stop();
        hideLoginStatus();
        m_linkGDBtn->setEnabled(true);
        showNotification(tr("Login was canceled"), UiConst::SettingsMessageState::None,
                         UiConst::SettingsTabNotiLevel::Caution);
        Q_EMIT cancelLoginRequested();
    });

    QObject::connect(m_cleanupEpubAfterChk, &QCheckBox::toggled, this, [this](bool isChecked) {
        if (m_expiredEpubSpbx) { m_expiredEpubSpbx->setEnabled(isChecked); }
    });
    QObject::connect(m_cleanupMDAfterChk, &QCheckBox::toggled, this, [this](bool isChecked) {
        if (m_expiredMDSpbx) { m_expiredMDSpbx->setEnabled(isChecked); }
    });
    QObject::connect(m_cleanupEpubCacheNowBtn, &QPushButton::clicked, [this] {
        m_cleanupEpubCacheNowBtn->setEnabled(false);
        Q_EMIT cleanupEpubCacheNowRequest();
    });
    QObject::connect(m_cleanupMDCacheNowBtn, &QPushButton::clicked, [this] {
        m_cleanupMDCacheNowBtn->setEnabled(false);
        Q_EMIT cleanupMDCacheNowRequest();
    });

    QObject::connect(m_regAssociationBtn, &QPushButton::clicked,
                     [this] { Q_EMIT onFileAssociationBtnClicked(); });

    // Gán lại thuộc tính động cho nút browse
    m_resDirBtn->setProperty("targetEdit", QVariant::fromValue(m_resDirInp));
}

void SettingsTabWidget::retranslateUi() {
    m_langLbl->setText(tr("Select language for GUI"));
    m_langEnRad->setText(tr("English"));
    m_langViRad->setText(tr("Vietnamese"));
    m_themeLbl->setText(tr("Select theme"));
    m_themeLightRad->setText(tr("Light"));
    m_themeDarkRad->setText(tr("Dark"));
    m_resDirLbl->setText(tr("Resource storage directory"));
    m_resManLbl->setText(tr("Notes file management type"));

    auto updateCombo = [this](UiConst::ResManKind kind, QString const& text) {
        int idx = m_resManCom->findData(QVariant::fromValue(kind));
        if (idx != -1) { m_resManCom->setItemText(idx, text); }
    };
    updateCombo(UiConst::ResManKind::Internal, tr("Internal"));
    updateCombo(UiConst::ResManKind::SavePathOnly, tr("Save path only"));

    m_applyBtn->setText(tr("Apply"));
    m_defaultBtn->setText(tr("Default"));
    m_linkGDBtn->setText(tr("Link Gmail for backup database to Google Drive"));
    m_uploadDBBtn->setText(tr("Upload"));
    m_downloadDBBtn->setText(tr("Download"));
    m_checkRemoteDBInfoBtn->setText(tr("Get DB info"));
    m_statusLabel->setText(tr("Waiting for you to confirm in the browser..."));
    m_info1->setText(tr("This app will:"));
    m_info2->setText(tr("• See your email address"));
    m_info3->setText(tr("• Create and manage backup files on your Google Drive"));
    m_info4->setText(tr("• Nothing else - no access to your other files"));
    m_cancelLoginBtn->setText(tr("Cancel login"));
}

void SettingsTabWidget::onApplyBtnClicked() {
    SettingsData data{};

    if (m_langEnRad->isChecked()) {
        data.language = UiConst::Language::English;
    } else if (m_langViRad->isChecked()) {
        data.language = UiConst::Language::Vietnamese;
    }

    if (m_themeLightRad->isChecked()) {
        data.theme = UiConst::Theme::Light;
    } else if (m_themeDarkRad->isChecked()) {
        data.theme = UiConst::Theme::Dark;
    }

    auto selectedKind = m_resManCom->currentData().value<UiConst::ResManKind>();
    data.isManagedResource = (selectedKind == UiConst::ResManKind::Internal);

    auto const path = m_resDirInp->text().trimmed();
    if (!path.isEmpty()) {
        data.resourceDir = std::filesystem::path(path.toStdWString());
        data.isResourceDirCustomized = true;
    }

    validateResourceDir(data);

    // Cleanup cache group
    data.isEpubCleanupCache = m_cleanupEpubAfterChk->isChecked();
    data.isMDCleanupCache = m_cleanupMDAfterChk->isChecked();
    data.expiredCleanupEpubCache = m_expiredEpubSpbx->value();
    data.expiredCleanupMDCache = m_expiredMDSpbx->value();

    Q_EMIT applySettingsRequested(data);
}

void SettingsTabWidget::onDefaultBtnClicked() {
    auto const reply =
        DialogUtils::showQuestion(this, tr("Restore Defaults"),
                                  tr("Do you want to restore default settings?\nChanges will not "
                                     "be saved until you click Apply."));

    if (reply == QMessageBox::Yes) { Q_EMIT defaultSettingsRequested(); }
}

void SettingsTabWidget::onBrowseBtnClicked() {
    auto* senderButton = qobject_cast<QPushButton*>(sender());
    if (senderButton == nullptr) { return; }

    auto* targetEdit = senderButton->property("targetEdit").value<QLineEdit*>();
    if (targetEdit == nullptr) { return; }

    auto& qSettings = SettingsManager::instance();
    QString const kDefaultDir =
        qSettings.get(QStringLiteral("settingsTab/lastBrowseDir"), QDir::homePath()).toString();

    QString dirPath =
        QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), kDefaultDir);

    if (!dirPath.isEmpty()) {
        QString const cleanPath = QDir::cleanPath(dirPath);
        targetEdit->setText(QDir::toNativeSeparators(cleanPath));

        // Get parent path
        QString const parentDir = QFileInfo(cleanPath).absoluteDir().absolutePath();
        qSettings.set(QStringLiteral("settingsTab/lastBrowseDir"), parentDir);
    }
}

// Language
QHBoxLayout* SettingsTabWidget::setupLanguageGroup() {
    auto* langLayout = new QHBoxLayout();
    auto* langRadioLayout = new QHBoxLayout();
    m_langLbl = new QLabel(tr("Select language for GUI"));
    auto* langGroup = new QButtonGroup(this);
    m_langEnRad = new QRadioButton(tr("English"));
    m_langViRad = new QRadioButton(tr("Vietnamese"));
    m_langEnRad->setChecked(true);
    langGroup->addButton(m_langEnRad);
    langGroup->addButton(m_langViRad);
    langRadioLayout->addWidget(m_langEnRad);
    langRadioLayout->addWidget(m_langViRad);
    langLayout->addWidget(m_langLbl);
    langLayout->addStretch(1);
    langLayout->addLayout(langRadioLayout);

    return langLayout;
}

// Theme
QHBoxLayout* SettingsTabWidget::setupThemeGroup() {
    auto* themeLayout = new QHBoxLayout();
    auto* themeRadioLayout = new QHBoxLayout();
    m_themeLbl = new QLabel(tr("Select theme"));
    auto* themeGroup = new QButtonGroup(this);
    m_themeLightRad = new QRadioButton(tr("Light"));
    m_themeDarkRad = new QRadioButton(tr("Dark"));
    m_themeLightRad->setChecked(true);
    themeGroup->addButton(m_themeLightRad);
    themeGroup->addButton(m_themeDarkRad);
    themeRadioLayout->addWidget(m_themeLightRad);
    themeRadioLayout->addWidget(m_themeDarkRad);
    themeLayout->addWidget(m_themeLbl);
    themeLayout->addStretch(1);
    themeLayout->addLayout(themeRadioLayout);

    return themeLayout;
}

// Resource dir
QHBoxLayout* SettingsTabWidget::setupResourceDirGroup() {
    auto* resDirLayout = new QHBoxLayout();
    m_resDirLbl = new QLabel(tr("Resource storage directory"));
    m_resDirInp = new QLineEdit();
    m_resDirInp->setText(QStringLiteral("resources"));
    m_resDirInp->setMaximumWidth(400); // NOLINT(readability-magic-numbers)
    m_resDirBtn = new QPushButton("...");
    m_resDirBtn->setMaximumWidth(UiConst::BUTTON_NEXT_INPUT_WIDTH);
    m_resDirBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_resDirInp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_resDirBtn->setProperty("targetEdit", QVariant::fromValue(m_resDirInp));

    resDirLayout->addWidget(m_resDirLbl);
    resDirLayout->addWidget(m_resDirInp);
    resDirLayout->addWidget(m_resDirBtn);
    resDirLayout->setSpacing(3);

    return resDirLayout;
}

// Resource Management Type
QHBoxLayout* SettingsTabWidget::setupResourceManagerTypeGroup() {
    auto* resManLayout = new QHBoxLayout();
    m_resManLbl = new QLabel(tr("Notes file management type"));
    m_resManCom = new QComboBox();
    m_resManCom->setMaximumWidth(200); // NOLINT(readability-magic-numbers)
    m_resManCom->addItem(tr("Internal"), QVariant::fromValue(UiConst::ResManKind::Internal));
    m_resManCom->addItem(tr("Save path only"),
                         QVariant::fromValue(UiConst::ResManKind::SavePathOnly));
    resManLayout->addWidget(m_resManLbl);
    resManLayout->addWidget(m_resManCom);

    return resManLayout;
}

QVBoxLayout* SettingsTabWidget::setupAccountLinkGroup() {
    auto* backupDBGroupLayout = new QVBoxLayout();

    auto* accountLinkLayout = new QHBoxLayout();
    m_linkGDBtn = new QPushButton(tr("Link Gmail for backup database to Google Drive"));
    m_linkGDBtn->setMaximumWidth(340); // NOLINT(readability-magic-numbers)
    m_addressUserGMLoginLbl = new QLabel();

    accountLinkLayout->addWidget(m_linkGDBtn);
    accountLinkLayout->addStretch(1);
    accountLinkLayout->addWidget(m_addressUserGMLoginLbl);

    auto* upDownButtonLayout = new QHBoxLayout();

    m_uploadDBBtn = new QPushButton(tr("Upload"));
    m_uploadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
    m_uploadDBBtn->setVisible(false);

    m_downloadDBBtn = new QPushButton(tr("Download"));
    m_downloadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
    m_downloadDBBtn->setVisible(false);

    m_checkRemoteDBInfoBtn = new QPushButton(tr("Get DB info"));
    m_downloadDBBtn->setMaximumWidth(100); // NOLINT(readability-magic-numbers)
    m_checkRemoteDBInfoBtn->setVisible(false);

    upDownButtonLayout->addWidget(m_uploadDBBtn);
    upDownButtonLayout->addWidget(m_downloadDBBtn);
    upDownButtonLayout->addWidget(m_checkRemoteDBInfoBtn);
    upDownButtonLayout->addStretch(1);
    upDownButtonLayout->setSpacing(10); // NOLINT(readability-magic-numbers)

    backupDBGroupLayout->addLayout(accountLinkLayout);
    backupDBGroupLayout->addLayout(upDownButtonLayout);
    backupDBGroupLayout->addWidget(setupLoginStatusGroup(), 0, Qt::AlignHCenter);

    return backupDBGroupLayout;
}

QWidget* SettingsTabWidget::setupLoginStatusGroup() {
    m_loginStatusWidget = new QWidget();
    m_loginStatusWidget->setVisible(false);

    auto* mainLayout = new QVBoxLayout(m_loginStatusWidget);
    mainLayout->setContentsMargins(0, 20, 0, 20); // NOLINT(readability-magic-numbers)

    m_statusLabel = new QLabel(tr("Waiting for you to confirm in the browser..."));
    m_statusLabel->setAlignment(Qt::AlignCenter);

    m_info1 = new QLabel(tr("This app will:"));
    m_info2 = new QLabel(tr("• See your email address"));
    m_info3 = new QLabel(tr("• Create and manage backup files on your Google Drive"));
    m_info4 = new QLabel(tr("• Nothing else - no access to your other files"));

    m_countdownLabel = new QLabel(QString::number(COUNTDOWN));
    m_countdownLabel->setStyleSheet("font-weight: bold; "
                                    "font-size: 24px; "
                                    "color: #d93025;");
    m_countdownLabel->setAlignment(Qt::AlignCenter);

    m_cancelLoginBtn = new QPushButton(tr("Cancel login"));

    mainLayout->addStretch(1);
    mainLayout->addWidget(m_statusLabel, 0, Qt::AlignHCenter);

    mainLayout->addWidget(m_info1);
    mainLayout->addWidget(m_info2);
    mainLayout->addWidget(m_info3);
    mainLayout->addWidget(m_info4);

    mainLayout->addSpacing(12); // NOLINT(readability-magic-numbers)

    mainLayout->addWidget(m_countdownLabel, 0, Qt::AlignHCenter);
    mainLayout->addWidget(m_cancelLoginBtn, 0, Qt::AlignHCenter);
    mainLayout->addStretch(1);

    return m_loginStatusWidget;
}

QHBoxLayout* SettingsTabWidget::setupButtonGroup() {
    // Thêm container chứa nhóm nút nằm ngang QHBoxLayout
    auto* buttonLayout = new QHBoxLayout();
    m_applyBtn = new QPushButton(tr("Apply"));
    m_applyBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_applyBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_applyBtn->setIcon(QIcon(":/icons/apply.ico"));
    m_defaultBtn = new QPushButton(tr("Default"));
    m_defaultBtn->setIcon(QIcon(":/icons/clear.ico"));
    m_defaultBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_defaultBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    // Layout nhóm nút nằm ngang và căn giữa nên thêm giãn trái và giãn phải
    buttonLayout->addStretch(1);  // Giãn bên trái
    buttonLayout->addWidget(m_applyBtn);
    buttonLayout->addWidget(m_defaultBtn);
    buttonLayout->addStretch(1);  // Giãn bên phải
    buttonLayout->setSpacing(10); // NOLINT(readability-magic-numbers)

    return buttonLayout;
}

QGroupBox* SettingsTabWidget::setupFileAssociation() {
    auto* fileAssociationGB = new QGroupBox();
    fileAssociationGB->setTitle(tr("File association .rvpk"));
    fileAssociationGB->setFlat(true);
    fileAssociationGB->setObjectName("GroupBox");

    auto* gridLayout = new QGridLayout(fileAssociationGB);

    gridLayout->setContentsMargins(15, 15, 15, 15); // NOLINT(readability-magic-numbers)
    gridLayout->setHorizontalSpacing(0);
    gridLayout->setVerticalSpacing(10);             // NOLINT(readability-magic-numbers)

    auto* statusTagLbl = new QLabel(tr("Status: "));
    m_associationStatusLbl = new QLabel(tr("Unregistered"));

    m_regAssociationBtn = new QPushButton(tr("Register"));
    m_regAssociationBtn->setFixedWidth(120); // NOLINT(readability-magic-numbers)

    gridLayout->addWidget(statusTagLbl, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    gridLayout->addWidget(m_associationStatusLbl, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);

    gridLayout->setColumnStretch(2, 1);

    gridLayout->addWidget(m_regAssociationBtn, 0, 3);

    return fileAssociationGB;
}

QGroupBox* SettingsTabWidget::setupCleanupGroup() {
    m_cleanupCacheGBox = new QGroupBox();
    m_cleanupCacheGBox->setTitle(tr("Cleanup EPUB && Markdown files cache"));
    m_cleanupCacheGBox->setFlat(true);
    m_cleanupCacheGBox->setObjectName("GroupBox");
    m_cleanupCacheGBox->setMinimumHeight(100);     // NOLINT(readability-magic-numbers)

    auto* mainLayout = new QVBoxLayout(m_cleanupCacheGBox);
    mainLayout->setContentsMargins(0, 20, 15, 15); // NOLINT(readability-magic-numbers)
    mainLayout->setSizeConstraint(QLayout::SetMinimumSize);

    auto createRow = [this](QString const& tagText, UiConst::CleanupMode mode) {
        auto* hLayout = new QHBoxLayout();
        hLayout->setSpacing(5); // NOLINT(readability-magic-numbers)

        auto* tagLbl = new QLabel(tagText);
        tagLbl->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
        tagLbl->setProperty("class", "TagLabel");

        QCheckBox* currentChk{};
        QSpinBox* currentSpbx{};
        QPushButton* currentBtn{};

        if (mode == UiConst::CleanupMode::Epub) {
            tagLbl->setProperty("mode", "epub");

            m_cleanupEpubAfterChk = new QCheckBox();
            m_expiredEpubSpbx = new QSpinBox();
            m_cleanupEpubCacheNowBtn = new QPushButton();

            currentChk = m_cleanupEpubAfterChk;
            currentSpbx = m_expiredEpubSpbx;
            currentBtn = m_cleanupEpubCacheNowBtn;
        } else if (mode == UiConst::CleanupMode::Markdown) {
            tagLbl->setProperty("mode", "markdown");

            m_cleanupMDAfterChk = new QCheckBox();
            m_expiredMDSpbx = new QSpinBox();
            m_cleanupMDCacheNowBtn = new QPushButton();

            currentChk = m_cleanupMDAfterChk;
            currentSpbx = m_expiredMDSpbx;
            currentBtn = m_cleanupMDCacheNowBtn;
        }

        currentChk->setText(tr("Cleanup files older than"));
        currentChk->setChecked(true);
        currentChk->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

        currentSpbx->setFixedWidth(90); // NOLINT(readability-magic-numbers)
        currentSpbx->setAlignment(Qt::AlignCenter);
        currentSpbx->setSuffix(" days");
        currentSpbx->setRange(1, 365);  // NOLINT(readability-magic-numbers)

        currentBtn->setText(tr("Cleanup now"));

        hLayout->addWidget(tagLbl);
        hLayout->addSpacing(10); // NOLINT(readability-magic-numbers)
        hLayout->addStretch(1);

        hLayout->addWidget(currentChk);
        hLayout->addWidget(currentSpbx);
        hLayout->addSpacing(25); // NOLINT(readability-magic-numbers)
        hLayout->addWidget(currentBtn);

        return hLayout;
    };

    mainLayout->addLayout(createRow("EPUB", UiConst::CleanupMode::Epub));
    mainLayout->setSpacing(15); // NOLINT(readability-magic-numbers)
    mainLayout->addLayout(createRow("MARKDOWN", UiConst::CleanupMode::Markdown));

    m_cleanupCacheGBox->setLayout(mainLayout);

    return m_cleanupCacheGBox;
}

void SettingsTabWidget::showNotification(QString const& message,
                                         UiConst::SettingsMessageState /*unused*/,
                                         UiConst::SettingsTabNotiLevel notiType) {
    if (m_notiSettingsChangedLbl == nullptr) { return; }

    m_notiSettingsChangedLbl->setText(message);
    m_notiSettingsChangedLbl->setVisible(true);

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

        QString objName = m_notiSettingsChangedLbl->objectName();
        if (objName.isEmpty()) {
            objName = "settingsTabNotiLbl";
            m_notiSettingsChangedLbl->setObjectName(objName);
        }
        m_notiSettingsChangedLbl->setStyleSheet(
            QString("#%1 { color: %2; }").arg(objName).arg(notiTextColor));
    }

    QTimer::singleShot(UiConst::NOTI_TIMEOUT, this, [this]() {
        m_notiSettingsChangedLbl->clear();
        m_notiSettingsChangedLbl->setVisible(false);
    });
}

void SettingsTabWidget::loadSettingsToUi(SettingsData const& settings) const {
    // Giao diện
    if (settings.theme == UiConst::Theme::Light) {
        m_themeLightRad->setChecked(true);
    } else if (settings.theme == UiConst::Theme::Dark) {
        m_themeDarkRad->setChecked(true);
    }

    // Ngôn ngữ
    if (settings.language == UiConst::Language::English) {
        m_langEnRad->setChecked(true);
    } else if (settings.language == UiConst::Language::Vietnamese) {
        m_langViRad->setChecked(true);
    }

    // Thư mục tài nguyên
    // m_resDirInp->setText(
    // QString::fromUtf8(reinterpret_cast<const char*>(settings.resourceDir.u8string().c_str())));
    auto const resDirPath = settings.resourceDir;
    m_resDirInp->setText(QString::fromStdU16String(resDirPath.u16string()));
    validateResourceDir(settings);

    // Kiểu quản lý tài nguyên
    auto currentKind = settings.isManagedResource ? UiConst::ResManKind::Internal
                                                  : UiConst::ResManKind::SavePathOnly;
    m_resManCom->setCurrentIndex(m_resManCom->findData(QVariant::fromValue(currentKind)));

    // Cleanup file cache group
    m_cleanupEpubAfterChk->setChecked(settings.isEpubCleanupCache);
    m_cleanupMDAfterChk->setChecked(settings.isMDCleanupCache);
    m_expiredEpubSpbx->setValue(settings.expiredCleanupEpubCache);
    m_expiredMDSpbx->setValue(settings.expiredCleanupMDCache);
}

void SettingsTabWidget::validateResourceDir(SettingsData const& settings) const {
    if (!settings.isManagedResource || !settings.isResourceDirCustomized) {
        m_resDirInp->setStyleSheet("");
        return;
    }

    bool const valid = std::filesystem::exists(settings.resourceDir) &&
                       std::filesystem::is_directory(settings.resourceDir);

    m_resDirInp->setStyleSheet(valid ? "" : R"(QLineEdit { border: 1px solid #e20c53; })");
}

void SettingsTabWidget::handleInitialSettingsLoad(SettingsData const& settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleSettingsStateChange(SettingsData const& settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleUiRefreshRequest(SettingsData const& settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleAfterLinkAccount(QString const& htmlTextEmail) {
    hideLoginStatus();

    m_addressUserGMLoginLbl->setText(htmlTextEmail);

    m_isGMAccountLinked = true;

    m_linkGDBtn->setEnabled(true);
    m_linkGDBtn->setText(tr("Unlink"));
    m_linkGDBtn->setMaximumWidth(100); // NOLINT(readability-magic-numbers)

    m_uploadDBBtn->setVisible(true);
    m_downloadDBBtn->setVisible(true);
    m_checkRemoteDBInfoBtn->setVisible(true);
}

void SettingsTabWidget::handleAfterUnlinkAccount() {
    m_isGMAccountLinked = false;
    m_addressUserGMLoginLbl->clear();
    m_linkGDBtn->setText(tr("Link Gmail for backup database to Google Drive"));
    m_linkGDBtn->setMaximumWidth(340); // NOLINT(readability-magic-numbers)

    m_uploadDBBtn->setVisible(false);
    m_downloadDBBtn->setVisible(false);
    m_checkRemoteDBInfoBtn->setVisible(false);
}

void SettingsTabWidget::onLinkBtnClicked() {
    if (!m_isGMAccountLinked) {
        // Chưa liên kết → yêu cầu AppController bắt đầu OAuth
        m_linkGDBtn->setEnabled(false);
        showLoginStatus();
        Q_EMIT requestGoogleLogin();

        return;
    }

    // Đã liên kết → Unlink
    auto reply = DialogUtils::showQuestion(
        this, tr("Information"),
        tr("Yep, you're unlinking your Google account! It'll be disconnected in a sec.\n\nBy the "
           "way, want to delete the data.db file on your Drive too, or leave it alone?"));

    Q_EMIT requestGoogleUnlink(reply == QMessageBox::Yes);
}

void SettingsTabWidget::handleLoginFailed(QString const& error) {
    hideLoginStatus();

    m_linkGDBtn->setEnabled(true);

    showNotification(error, UiConst::SettingsMessageState::None,
                     UiConst::SettingsTabNotiLevel::Warning);
}

void SettingsTabWidget::handleUploadDBRequested(bool isDisable, QString const& message,
                                                UiConst::SettingsTabNotiLevel notiType) {
    if (m_uploadDBBtn != nullptr) {
        if (isDisable) {
            m_uploadDBBtn->setMaximumWidth(100); // NOLINT(readability-magic-numbers)
            m_uploadDBBtn->setEnabled(false);
            m_uploadDBBtn->setText(tr("Uploading..."));
        } else {
            m_uploadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
            m_uploadDBBtn->setEnabled(true);
            m_uploadDBBtn->setText(tr("Upload"));

            showNotification(message, UiConst::SettingsMessageState::None, notiType);
        }
    }
}

void SettingsTabWidget::handleDownloadDBRequested(bool isDisable, QString const& message,
                                                  UiConst::SettingsTabNotiLevel notiType) {
    if (m_downloadDBBtn != nullptr) {
        if (isDisable) {
            m_downloadDBBtn->setMaximumWidth(120); // NOLINT(readability-magic-numbers)
            m_downloadDBBtn->setEnabled(false);
            m_downloadDBBtn->setText(tr("Downloading..."));
        } else {
            m_downloadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
            m_downloadDBBtn->setEnabled(true);
            m_downloadDBBtn->setText(tr("Download"));

            showNotification(message, UiConst::SettingsMessageState::None, notiType);
        }
    }
}

void SettingsTabWidget::onUploadButtonClicked() {
    auto const reply =
        DialogUtils::showQuestion(this, tr("Upload database to Google Drive"),
                                  tr("Do you want to upload <b>data.db</b> to Google "
                                     "Drive?<br><br>The file will be compacted locally "
                                     "and will replace the existing one on Drive."),
                                  true);

    if (reply == QMessageBox::Yes) { Q_EMIT requestUpload(); }
}

void SettingsTabWidget::onDownloadButtonClicked() {
    auto const reply = DialogUtils::showQuestion(
        this, tr("Download database from Google Drive"),
        tr("Do you want to download the file <b>data.db</b> from the linked Google "
           "Drive?<br><br>This will overwrite the <b>data.db</b> file currently used by this "
           "application."),
        true);

    if (reply == QMessageBox::Yes) { Q_EMIT requestDownload(); }
}

void SettingsTabWidget::updateCountdownDisplay() {
    m_remainingSeconds--;

    if (m_remainingSeconds <= 0) {
        m_countdownLabel->setText("0");
        m_countdownTimer.stop();
        return;
    }

    m_countdownLabel->setText(QString::number(m_remainingSeconds));
}

void SettingsTabWidget::hideLoginStatus() {
    m_countdownTimer.stop();
    m_countdownTimer.disconnect();

    m_loginStatusWidget->setVisible(false);

    m_remainingSeconds = 0;
}

void SettingsTabWidget::showLoginStatus() {
    m_remainingSeconds = COUNTDOWN; // NOLINT(readability-magic-numbers)
    m_countdownLabel->setText(QString::number(COUNTDOWN));

    m_countdownTimer.disconnect();  // tránh chồng signal
    QObject::connect(&m_countdownTimer, &QTimer::timeout, this,
                     &SettingsTabWidget::updateCountdownDisplay);
    m_countdownTimer.start(1000);   // NOLINT(readability-magic-numbers)

    m_loginStatusWidget->setVisible(true);
}

void SettingsTabWidget::handleDBInfoGot(QStringList const& info) {
    m_checkRemoteDBInfoBtn->setEnabled(true);

    if (info.isEmpty()) {
        DialogUtils::showInfo(this, tr("Database information"), tr("Database not exist"));
    } else {
        DialogUtils::showInfo(
            this, tr("Database information"),
            tr("File: %1\nVersion: %2\nSize: %3\nLast created: %4\nLast modified: %5")
                .arg(info[0])
                .arg(info[1])
                .arg(info[2])
                .arg(info[3])
                .arg(info[4]));
    }
}

void SettingsTabWidget::handleDeleteDBFileRespond(QString const& msg) {
    DialogUtils::showInfo(this, tr("Information"), msg);
}

void SettingsTabWidget::handleButtonAfterCleanup(UiConst::CleanupMode mode) {
    if (mode == UiConst::CleanupMode::Epub) {
        m_cleanupEpubCacheNowBtn->setEnabled(true);
    } else if (mode == UiConst::CleanupMode::Markdown) {
        m_cleanupMDCacheNowBtn->setEnabled(true);
    }
}

void SettingsTabWidget::handleFileAssociationStatus(bool isRegistered) {
    m_regAssociationBtn->setText(isRegistered ? tr("Unregister") : tr("Register"));
    m_associationStatusLbl->setText(isRegistered ? tr("Registered") : tr("Unregistered"));
    m_associationStatusLbl->setStyleSheet(isRegistered ? "color: green;" : "");
}
