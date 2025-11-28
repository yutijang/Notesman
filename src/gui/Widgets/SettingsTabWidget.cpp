#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QVariant>
#include <QDir>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QFileDialog>

#include "SettingsTabWidget.hpp"
#include "UiConstants.hpp"
#include "SettingsData.hpp"

namespace {
    constexpr int LAYOUT_MINWIDTH{370};
    constexpr int COUNTDOWN{60};
    constexpr auto SETTINGS_ORG = "Notesman";
    constexpr auto SETTINGS_APP = "configs";
} // namespace

SettingsTabWidget::SettingsTabWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    setupConnections();
}

void SettingsTabWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);

    auto* contentWidget = new QWidget();
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentWidget->setMinimumWidth(LAYOUT_MINWIDTH);

    // Nhãn thông báo cập nhật Settings
    m_notiSettingsChangedLbl = new QLabel("");
    m_notiSettingsChangedLbl->setAlignment(Qt::AlignCenter);
    m_notiSettingsChangedLbl->setVisible(false);

    contentLayout->addLayout(setupLanguageGroup());
    contentLayout->addLayout(setupThemeGroup());
    contentLayout->addLayout(setupResourceDirGroup());
    contentLayout->addLayout(setupResourceManagerTypeGroup());
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

    QObject::connect(m_linkGDBtn, &QPushButton::clicked, this,
                     &SettingsTabWidget::onLinkBtnClicked);

    QObject::connect(m_cancelLoginBtn, &QPushButton::clicked, this, [this]() {
        m_countdownTimer.stop();
        hideLoginStatus();
        m_linkGDBtn->setEnabled(true);
        showNotification(tr("Login was canceled"));
        emit cancelLoginRequested();
    });

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
    m_resManCom->setItemText(0, tr("Internal"));
    m_resManCom->setItemText(1, tr("Save path only"));
    m_applyBtn->setText(tr("Apply"));
    m_defaultBtn->setText(tr("Default"));
}

void SettingsTabWidget::onApplyBtnClicked() {
    SettingsData data{};

    if (m_langEnRad->isChecked()) {
        data.language = Language::english;
    } else if (m_langViRad->isChecked()) {
        data.language = Language::vietnamese;
    }

    if (m_themeLightRad->isChecked()) {
        data.theme = Theme::light;
    } else if (m_themeDarkRad->isChecked()) {
        data.theme = Theme::dark;
    }

    auto path = m_resDirInp->text().trimmed();
    if (!path.isEmpty()) { data.resourceDir = path.toStdString(); }

    data.isManagedResource = m_resManCom->currentData().toBool();

    emit applySettingsRequested(data);
}

void SettingsTabWidget::onDefaultBtnClicked() {
    const auto reply = QMessageBox::question(this, tr("Restore Defaults"),
                                             tr("Do you want to restore default settings?\nChanges "
                                                "will not be saved until you click Apply."),
                                             QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) { emit defaultSettingsRequested(); }
}

void SettingsTabWidget::onBrowseBtnClicked() {
    auto* senderButton = qobject_cast<QPushButton*>(sender());
    if (senderButton == nullptr) { return; }

    auto* targetEdit = senderButton->property("targetEdit").value<QLineEdit*>();
    if (targetEdit == nullptr) { return; }

    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    const QString kDefaultDir =
        settings.value(QStringLiteral("settingsTab/lastBrowseDir"), QDir::homePath()).toString();

    QString dirPath =
        QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), kDefaultDir);

    if (!dirPath.isEmpty()) {
        const QString cleanPath = QDir::cleanPath(dirPath);
        targetEdit->setText(QDir::toNativeSeparators(cleanPath));

        // Get parent path
        const QString parentDir = QFileInfo(cleanPath).absoluteDir().absolutePath();
        settings.setValue(QStringLiteral("settingsTab/lastBrowseDir"), parentDir);
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
    m_resDirInp->setText(QStringLiteral("resources/"));
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
    m_resManCom->addItem(tr("Internal"), QVariant::fromValue(true));
    m_resManCom->addItem(tr("Save path only"), QVariant::fromValue(false));
    resManLayout->addWidget(m_resManLbl);
    resManLayout->addWidget(m_resManCom);

    return resManLayout;
}

QVBoxLayout* SettingsTabWidget::setupAccountLinkGroup() {
    auto* backupDBGroupLayout = new QVBoxLayout();

    auto* accountLinkLayout = new QHBoxLayout();
    m_linkGDBtn = new QPushButton(tr("Link Gmail for backup database to Google Drive"));
    m_linkGDBtn->setMaximumWidth(300); // NOLINT(readability-magic-numbers)
    m_addressUserGMLoginLbl = new QLabel("");

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

    upDownButtonLayout->addWidget(m_uploadDBBtn);
    upDownButtonLayout->addWidget(m_downloadDBBtn);
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

    auto* info1 = new QLabel(tr("This app will:"));
    auto* info2 = new QLabel(tr("• See your email address"));
    auto* info3 = new QLabel(tr("• Create and manage backup files on your Google Drive"));
    auto* info4 = new QLabel(tr("• Nothing else – no access to your other files"));

    m_countdownLabel = new QLabel(QString::number(COUNTDOWN));
    m_countdownLabel->setStyleSheet("font-weight: bold; "
                                    "font-size: 24px; "
                                    "color: #d93025;");
    m_countdownLabel->setAlignment(Qt::AlignCenter);

    m_cancelLoginBtn = new QPushButton(tr("Cancel login"));

    mainLayout->addStretch(1);
    mainLayout->addWidget(m_statusLabel, 0, Qt::AlignHCenter);

    mainLayout->addWidget(info1);
    mainLayout->addWidget(info2);
    mainLayout->addWidget(info3);
    mainLayout->addWidget(info4);

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

void SettingsTabWidget::showNotification(const QString &message) {
    if (m_notiSettingsChangedLbl == nullptr) { return; }

    m_notiSettingsChangedLbl->setText(message);
    m_notiSettingsChangedLbl->setVisible(true);

    QTimer::singleShot(UiConst::NOTI_TIMEOUT, this, [this]() {
        m_notiSettingsChangedLbl->clear();
        m_notiSettingsChangedLbl->setVisible(false);
    });
}

void SettingsTabWidget::loadSettingsToUi(const SettingsData &settings) const {
    // Giao diện
    if (settings.theme == Theme::light) {
        m_themeLightRad->setChecked(true);
    } else if (settings.theme == Theme::dark) {
        m_themeDarkRad->setChecked(true);
    }

    // Ngôn ngữ
    if (settings.language == Language::english) {
        m_langEnRad->setChecked(true);
    } else if (settings.language == Language::vietnamese) {
        m_langViRad->setChecked(true);
    }

    // Thư mục tài nguyên
    // m_resDirInp->setText(
    // QString::fromUtf8(reinterpret_cast<const char*>(settings.resourceDir.u8string().c_str())));
    m_resDirInp->setText(QString::fromStdU16String(settings.resourceDir.u16string()));

    // Kiểu quản lý tài nguyên
    if (settings.isManagedResource) {
        m_resManCom->setCurrentIndex(0); // "Internal"
    } else {
        m_resManCom->setCurrentIndex(1); // "Save path only"
    }
}

void SettingsTabWidget::handleInitialSettingsLoad(const SettingsData &settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleSettingsStateChange(const SettingsData &settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleUiRefreshRequest(const SettingsData &settings) const {
    loadSettingsToUi(settings);
}

void SettingsTabWidget::handleAfterLinkAccount(const QString &htmlTextEmail) {
    hideLoginStatus();

    m_addressUserGMLoginLbl->setText(htmlTextEmail);

    m_isLinked = true;

    m_linkGDBtn->setEnabled(true);
    m_linkGDBtn->setText("Unlink");
    m_linkGDBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)

    m_uploadDBBtn->setVisible(true);
    m_downloadDBBtn->setVisible(true);
}

void SettingsTabWidget::handleAfterUnlinkAccount() {
    m_isLinked = false;
    m_addressUserGMLoginLbl->clear();
    m_linkGDBtn->setText("Link Gmail for backup database to Google Drive");
    m_linkGDBtn->setMaximumWidth(300); // NOLINT(readability-magic-numbers)

    m_uploadDBBtn->setVisible(false);
    m_downloadDBBtn->setVisible(false);
}

void SettingsTabWidget::onLinkBtnClicked() {
    if (!m_isLinked) {
        // Chưa liên kết → yêu cầu AppController bắt đầu OAuth
        m_linkGDBtn->setEnabled(false);
        showLoginStatus();
        emit requestGoogleLogin();

        return;
    }

    // Đã liên kết → Unlink
    emit requestGoogleUnlink();
}

void SettingsTabWidget::handleLoginFailed(const QString &error) {
    hideLoginStatus();

    m_linkGDBtn->setEnabled(true);

    showNotification(error);
}

void SettingsTabWidget::handleUploadDBRequested(bool isDisable) {
    if (m_uploadDBBtn != nullptr) {
        if (isDisable) {
            m_uploadDBBtn->setMaximumWidth(100); // NOLINT(readability-magic-numbers)
            m_uploadDBBtn->setEnabled(false);
            m_uploadDBBtn->setText(tr("Uploading..."));
        } else {
            m_uploadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
            m_uploadDBBtn->setEnabled(true);
            m_uploadDBBtn->setText(tr("Upload"));

            showNotification(tr("Upload completed"));
        }
    }
}

void SettingsTabWidget::handleDownloadDBRequested(bool isDisable) {
    if (m_downloadDBBtn != nullptr) {
        if (isDisable) {
            m_downloadDBBtn->setMaximumWidth(120); // NOLINT(readability-magic-numbers)
            m_downloadDBBtn->setEnabled(false);
            m_downloadDBBtn->setText(tr("Downloading..."));
        } else {
            m_downloadDBBtn->setMaximumWidth(80); // NOLINT(readability-magic-numbers)
            m_downloadDBBtn->setEnabled(true);
            m_downloadDBBtn->setText(tr("Download"));

            showNotification(tr("Download completed"));
        }
    }
}

void SettingsTabWidget::onUploadButtonClicked() {
    const auto reply = QMessageBox::question(
        this, tr("Upload database to Google Drive"),
        tr("Do you want to upload data.db to the linked Google Drive?\nThis will overwrite the "
           "existing data.db file on Google Drive if it already exists."),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) { emit requestUpload(); }
}

void SettingsTabWidget::onDownloadButtonClicked() {
    const auto reply = QMessageBox::question(
        this, tr("Download database from Google Drive"),
        tr("Do you want to download the file data.db from the linked Google Drive?\nThis will "
           "overwrite the data.db file currently used by this application."),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) { emit requestDownload(); }
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
