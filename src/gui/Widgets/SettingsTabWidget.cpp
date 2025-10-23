#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QVariant>
#include "SettingsTabWidget.hpp"
#include "UiConstants.hpp"

namespace {
    constexpr int LAYOUT_MINWIDTH{370};
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
    connect(m_applyBtn, &QPushButton::clicked, this, &SettingsTabWidget::onApplyBtnClicked);
    connect(m_defaultBtn, &QPushButton::clicked, this, &SettingsTabWidget::onDefaultBtnClicked);
    connect(m_resDirBtn, &QPushButton::clicked, this, &SettingsTabWidget::onBrowseBtnClicked);

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
    m_resManCom->setItemText(0, tr("Notes Manager"));
    m_resManCom->setItemText(1, tr("Save path only"));
    m_applyBtn->setText(tr("Apply"));
    m_defaultBtn->setText(tr("Default"));
}

void SettingsTabWidget::onApplyBtnClicked() {
    emit applyClicked();
}

void SettingsTabWidget::onDefaultBtnClicked() {
    emit defaultClicked();
}

void SettingsTabWidget::onBrowseBtnClicked() {
    // Phát tín hiệu để MainWindow gọi hàm pickupFolder() chung
    emit resourceDirBrowseRequested();
}

// Getters
QRadioButton* SettingsTabWidget::langEnRad() const noexcept {
    return m_langEnRad;
}

QRadioButton* SettingsTabWidget::langViRad() const noexcept {
    return m_langViRad;
}

QRadioButton* SettingsTabWidget::themeLightRad() const noexcept {
    return m_themeLightRad;
}

QRadioButton* SettingsTabWidget::themeDarkRad() const noexcept {
    return m_themeDarkRad;
}

QLineEdit* SettingsTabWidget::resourceDirInput() const noexcept {
    return m_resDirInp;
}

QComboBox* SettingsTabWidget::resourceManagementCombo() const noexcept {
    return m_resManCom;
}

QLabel* SettingsTabWidget::notificationLabel() const noexcept {
    return m_notiSettingsChangedLbl;
}

QPushButton* SettingsTabWidget::applyButton() const noexcept {
    return m_applyBtn;
}

QPushButton* SettingsTabWidget::defaultButton() const noexcept {
    return m_defaultBtn;
}

QHBoxLayout* SettingsTabWidget::setupLanguageGroup() {
    // Language
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

QHBoxLayout* SettingsTabWidget::setupThemeGroup() {
    // Theme
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

QHBoxLayout* SettingsTabWidget::setupResourceDirGroup() {
    // Resource dir
    auto* resDirLayout = new QHBoxLayout();
    m_resDirLbl = new QLabel(tr("Resource storage directory"));
    m_resDirInp = new QLineEdit();
    m_resDirInp->setText("resources/");
    m_resDirInp->setMaximumWidth(400); // NOLINT(readability-magic-numbers)
    m_resDirBtn = new QPushButton("...");
    m_resDirBtn->setMaximumWidth(40);  // NOLINT(readability-magic-numbers)
    m_resDirBtn->setProperty("targetEdit", QVariant::fromValue(m_resDirInp));

    resDirLayout->addWidget(m_resDirLbl);
    resDirLayout->addWidget(m_resDirInp);
    resDirLayout->addWidget(m_resDirBtn);
    resDirLayout->setSpacing(3);

    return resDirLayout;
}

QHBoxLayout* SettingsTabWidget::setupResourceManagerTypeGroup() {
    // Resource Management Type
    auto* resManLayout = new QHBoxLayout();
    m_resManLbl = new QLabel(tr("Notes file management type"));
    m_resManCom = new QComboBox();
    m_resManCom->setMaximumWidth(200); // NOLINT(readability-magic-numbers)
    m_resManCom->addItem(tr("Notes Manager"), QVariant::fromValue(true));
    m_resManCom->addItem(tr("Save path only"), QVariant::fromValue(false));
    resManLayout->addWidget(m_resManLbl);
    resManLayout->addWidget(m_resManCom);

    return resManLayout;
}

QHBoxLayout* SettingsTabWidget::setupButtonGroup() {
    // Thêm container chứa nhóm nút nằm ngang QHBoxLayout
    auto* buttonLayout = new QHBoxLayout();
    m_applyBtn = new QPushButton(tr("Apply"));
    m_applyBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_applyBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_applyBtn->setIcon(QIcon(":/icons/apply.ico"));
    m_defaultBtn = new QPushButton(tr("Default"));
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
