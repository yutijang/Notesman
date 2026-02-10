#include <cstddef>
#include <qminmax.h>
#include <vector>
#include <optional>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QHeaderView>
#include <QTableView>
#include <QGroupBox>
#include <QSizePolicy>
#include <Qt>
#include <QAbstractItemView>
#include <QObject>
#include <QTableWidget>
#include <QtGlobal>
#include <QComboBox>
#include <QtAssert>
#include <QtTypes>

#include "BrowseTabWidget.hpp"
#include "Logger.hpp"
#include "ResultsTable.hpp"
#include "model.hpp"
#include "UiConstants.hpp"
#include "ResourceTitleDelegate.hpp"

BrowseTabWidget::BrowseTabWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
    setupConnections();
}

void BrowseTabWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);                 // NOLINT(readability-magic-numbers)
    topLayout->setContentsMargins(0, 0, 0, 5); // NOLINT(readability-magic-numbers)

    auto* searchGroupLayout = new QVBoxLayout();
    searchGroupLayout->setSpacing(5);          // NOLINT(readability-magic-numbers)
    searchGroupLayout->setContentsMargins(0, 0, 0, 0);

    searchGroupLayout->addLayout(setupSearchInpLayoutGroup());
    searchGroupLayout->addLayout(setupRadioLayoutGroup());
    searchGroupLayout->addStretch(1);

    topLayout->addLayout(searchGroupLayout, 2);
    topLayout->addWidget(setuputilityContainerGroup(), 3);

    setupResultTableGroup();

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_resultsTbl, 1);
}

void BrowseTabWidget::setupConnections() {
    QObject::connect(m_searchBtn, &QPushButton::clicked, this,
                     &BrowseTabWidget::onSearchButtonClicked);

    QObject::connect(m_searchInp, &QLineEdit::returnPressed, m_searchBtn, &QPushButton::click);

    QObject::connect(m_resultsTbl, &QTableWidget::cellDoubleClicked, this,
                     &BrowseTabWidget::onCellDoubleClicked);

    QObject::connect(m_resultsTbl, &QWidget::customContextMenuRequested, this,
                     &BrowseTabWidget::onCustomContextMenuRequested);

    QObject::connect(m_clearTableBtn, &QPushButton::clicked, this,
                     &BrowseTabWidget::onClearButtonClicked);

    QObject::connect(m_loadAllBtn, &QPushButton::clicked, this,
                     &BrowseTabWidget::loadAllDataRequested);

    QObject::connect(m_loadResTypeBtn, &QPushButton::clicked, [this] {
        Q_EMIT loadResourceByTypeRequested(currentResourceType(m_getResTypeCom));
    });
}

void BrowseTabWidget::retranslateUi() {
    m_searchBtn->setText(tr("Search"));
    m_searchInp->setPlaceholderText(tr("Enter keyword..."));
    m_searchByLbl->setText(tr("Search by: "));
    m_titleRad->setText(tr("Title"));
    m_contentRad->setText(tr("Content"));
    m_allRad->setText(tr("All"));

    m_resultsTbl->setHorizontalHeaderLabels({tr("No."), tr("Title")});

    m_loadResTypeBtn->setText(tr("Load"));
    m_loadAllBtn->setText(tr("Load All"));
    m_clearTableBtn->setText(tr("Clear"));

    populateResourceTypeCombo(m_getResTypeCom);
}

void BrowseTabWidget::updateColumnWidths() {
    if (m_resultsTbl == nullptr) { return; }

    const int leftRightMargin{20};
    int tableWidth = m_resultsTbl->viewport()->width() - leftRightMargin;
    const int idWidth{50};
    int remaining = qMax(0, tableWidth - idWidth);
    m_resultsTbl->setColumnWidth(0, idWidth);
    m_resultsTbl->setColumnWidth(1, remaining / 3);
}

void BrowseTabWidget::displayResults(const std::vector<UnifiedSearchResult> &results) {
    if (results.empty()) { return; }

    m_resultsTbl->setRowCount(0); // Dọn dẹp (clear) hoặc chuẩn bị lại bảng kết quả

    m_resultsTbl->setUpdatesEnabled(false);
    m_resultsTbl->setSortingEnabled(false);

    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto &res = results[i];
        const int row = m_resultsTbl->rowCount();
        m_resultsTbl->insertRow(row);

        // No.
        auto* noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        noItem->setFlags(noItem->flags() & ~Qt::ItemIsEditable);
        m_resultsTbl->setItem(row, 0, noItem);

        // Title (delegate sẽ vẽ icon)
        auto* titleItem = new QTableWidgetItem(QString::fromStdString(res.res.title));
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);

        titleItem->setData(static_cast<int>(ResultsTable::ItemRole::ResourceId),
                           QVariant::fromValue<qlonglong>(res.res.id));

        titleItem->setData(static_cast<int>(ResultsTable::ItemRole::ResourceType),
                           static_cast<int>(res.res.type));

        titleItem->setData(static_cast<int>(ResultsTable::ItemRole::DisplaySubText),
                           QString::fromStdString(res.displaySubText));

        titleItem->setData(static_cast<int>(ResultsTable::ItemRole::ResourceFlags),
                           QVariant::fromValue(static_cast<int>(res.flags)));

        if (hasFlag(res.flags, ResourceFlags::IsFile) && res.filePath.has_value()) {
            titleItem->setData(static_cast<int>(ResultsTable::ItemRole::FullPath),
                               QString::fromStdString(*res.filePath));
        }

        if (res.url) {
            titleItem->setData(static_cast<int>(ResultsTable::ItemRole::Url),
                               QString::fromStdString(*res.url));
        }

        m_resultsTbl->setItem(row, 1, titleItem);
    }

    m_resultsTbl->setUpdatesEnabled(true);
    m_resultsTbl->setSortingEnabled(true);

    Q_EMIT statusUpdateRequest(tr("Found %1 results").arg(QString::number(results.size())),
                               UiConst::NOTI_TIMEOUT);
}

// signals custom
void BrowseTabWidget::onCellDoubleClicked(int row) {
    auto rowDataOpt = rowData(row);
    if (!rowDataOpt.has_value()) {
        Log::warn("Invalid row: {}", row);
        return;
    }

    const auto &data = *rowDataOpt;

    Q_EMIT resourceDoubleClicked(data.id, data.type, data.title, data.path, data.url);
}

void BrowseTabWidget::onCustomContextMenuRequested(const QPoint &pos) {
    const QModelIndex index = m_resultsTbl->indexAt(pos);
    if (!index.isValid()) { return; }

    const int row = index.row();

    auto rowDataOpt = rowData(row);
    if (!rowDataOpt.has_value()) {
        Log::warn("Invalid row: {}", row);
        return;
    }

    const auto &data = *rowDataOpt;

    Q_EMIT contextMenuRequested(pos, data.id, data.type, data.title, data.path, data.url);
}

std::optional<BrowseTabWidget::RowData> BrowseTabWidget::rowData(int row) const {
    if (row < 0 || row >= m_resultsTbl->rowCount()) { return std::nullopt; }

    auto* titleItem = m_resultsTbl->item(row, 1);
    if (titleItem == nullptr) { return std::nullopt; }

    const QVariant idVar = titleItem->data(static_cast<int>(ResultsTable::ItemRole::ResourceId));
    if (!idVar.isValid()) { return std::nullopt; }
    const auto id = static_cast<int>(idVar.toLongLong());

    const QVariant vRes = titleItem->data(static_cast<int>(ResultsTable::ItemRole::ResourceType));
    bool ok{};
    const int raw = vRes.toInt(&ok);
    if (!ok) { return std::nullopt; }
    const auto type = static_cast<ResourceType>(raw);

    const QVariant pathVar = titleItem->data(static_cast<int>(ResultsTable::ItemRole::FullPath));
    QString path;
    if (pathVar.isValid() && !pathVar.isNull()) { path = pathVar.toString(); }

    const QVariant urlVar = titleItem->data(static_cast<int>(ResultsTable::ItemRole::Url));
    QString url;
    if (urlVar.isValid() && !urlVar.isNull()) { url = urlVar.toString(); }

    RowData r{.id = id, .type = type, .title = titleItem->text(), .path = path, .url = url};

    return r;
}

void BrowseTabWidget::onClearButtonClicked() {
    m_resultsTbl->setSortingEnabled(false);
    m_resultsTbl->clearContents();
    m_resultsTbl->setRowCount(0);
    m_resultsTbl->clearSelection();
    m_resultsTbl->setSortingEnabled(true);
}

void BrowseTabWidget::onGetAllButtonClicked() {
    Q_EMIT loadAllDataRequested();
}

void BrowseTabWidget::onSearchButtonClicked() {
    // IIFE: Biểu thức lambda được định nghĩa và gọi ngay lập tức ()
    const QString mode = [this]() -> QString {
        if (m_titleRad->isChecked()) { return "title"; }
        if (m_contentRad->isChecked()) { return "content"; }
        if (m_tagRad->isChecked()) { return "tag"; }
        // Luôn phải có return cuối cùng cho các trường hợp còn lại
        return "all";
    }(); // Dấu ngoặc () ở cuối để gọi lambda ngay lập tức

    Q_EMIT searchRequested(m_searchInp->text().trimmed(), mode);
}

void BrowseTabWidget::handleResultsSearchRequested(
    const std::vector<UnifiedSearchResult> &results) {
    displayResults(results);
    updateColumnWidths();
}

ResourceType BrowseTabWidget::currentResourceType(const QComboBox* combo) {
    return static_cast<ResourceType>(combo->currentData().toInt());
}

void BrowseTabWidget::populateResourceTypeCombo(QComboBox* combo) {
    combo->clear();

    for (const auto &meta : K_RESOURCE_TYPE_TABLE) {
        combo->addItem(resourceTypeToDisplayText(meta.type), static_cast<int>(meta.type));
    }
}

// Group searchInpLayout
QHBoxLayout* BrowseTabWidget::setupSearchInpLayoutGroup() {
    auto* searchInpLayout = new QHBoxLayout();

    m_searchInp = new QLineEdit();
    m_searchInp->setPlaceholderText(tr("Enter keyword..."));
    m_searchBtn = new QPushButton(tr("Search"));
    m_searchBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_searchInp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_searchBtn->setIcon(QIcon(":/icons/search_button.ico"));
    searchInpLayout->addWidget(m_searchInp);
    searchInpLayout->addWidget(m_searchBtn);
    searchInpLayout->setSpacing(3);

    return searchInpLayout;
}

// Radio search group
QHBoxLayout* BrowseTabWidget::setupRadioLayoutGroup() {
    auto* radioLayout = new QHBoxLayout();

    radioLayout->addStretch(1);
    m_searchByLbl = new QLabel(tr("Search by: "));
    m_titleRad = new QRadioButton(tr("Title"));
    m_contentRad = new QRadioButton(tr("Content"));
    m_tagRad = new QRadioButton("Tag");
    m_allRad = new QRadioButton(tr("All"));
    m_titleRad->setChecked(true);
    auto* searchByGroup = new QButtonGroup(this);
    searchByGroup->addButton(m_titleRad);
    searchByGroup->addButton(m_contentRad);
    searchByGroup->addButton(m_tagRad);
    searchByGroup->addButton(m_allRad);
    radioLayout->addWidget(m_searchByLbl);
    radioLayout->addWidget(m_titleRad);
    radioLayout->addWidget(m_contentRad);
    radioLayout->addWidget(m_tagRad);
    radioLayout->addWidget(m_allRad);
    radioLayout->setSpacing(20); // NOLINT(readability-magic-numbers)
    radioLayout->addStretch(1);

    return radioLayout;
}

QWidget* BrowseTabWidget::setuputilityContainerGroup() {
    auto* utilityContainer = new QWidget();

    auto* utilityLayout = new QVBoxLayout(utilityContainer);
    utilityLayout->setSpacing(8);                   // NOLINT(readability-magic-numbers)
    utilityLayout->setContentsMargins(30, 0, 0, 5); // NOLINT(readability-magic-numbers)

    auto* getResGroupLayout = new QHBoxLayout();
    m_getResTypeCom = new QComboBox();
    m_getResTypeCom->setMinimumWidth(150); // NOLINT(readability-magic-numbers)
    populateResourceTypeCombo(m_getResTypeCom);

    m_loadResTypeBtn = new QPushButton(tr("Load"));
    m_loadResTypeBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_loadAllBtn = new QPushButton(tr("Load All"));
    m_loadAllBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    m_clearTableBtn = new QPushButton(tr("Clear"));
    m_clearTableBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);

    m_getResTypeCom->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_loadResTypeBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    getResGroupLayout->addWidget(m_getResTypeCom);
    getResGroupLayout->addWidget(m_loadResTypeBtn);
    getResGroupLayout->addWidget(m_loadAllBtn);
    getResGroupLayout->addWidget(m_clearTableBtn);
    getResGroupLayout->addStretch(1);

    utilityLayout->addLayout(getResGroupLayout);
    utilityLayout->addStretch(1);

    return utilityContainer;
}

void BrowseTabWidget::setupResultTableGroup() {
    m_resultsTbl = new ResultsTable(this);
    m_resultsTbl->verticalHeader()->setDefaultSectionSize(48); // NOLINT(readability-magic-numbers)
    m_resultsTbl->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_resultsTbl->setContextMenuPolicy(Qt::CustomContextMenu);
    m_resultsTbl->setColumnCount(2);
    m_resultsTbl->setHorizontalHeaderLabels({tr("No."), tr("Title")});
    m_resultsTbl->horizontalHeader()->setStretchLastSection(true);
    m_resultsTbl->verticalHeader()->setVisible(false);
    m_resultsTbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_resultsTbl->setColumnWidth(0, 50); // NOLINT(readability-magic-numbers)
    m_resultsTbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTbl->setSortingEnabled(true);
    m_resultsTbl->setItemDelegateForColumn(1, new ResourceTitleDelegate(m_resultsTbl));
}

QString BrowseTabWidget::resourceTypeToDisplayText(ResourceType type) {
    switch (type) {
        case ResourceType::PlainText: return tr("Text");
        case ResourceType::CCppCode : return tr("C/C++ Code");
        case ResourceType::Markdown : return tr("Markdown Document");
        case ResourceType::HtmlDoc  : return tr("HTML Document");
        case ResourceType::PdfDoc   : return tr("PDF Document");
        case ResourceType::EpubDoc  : return tr("EPUB Document");
        case ResourceType::Url      : return tr("Web Document");
        case ResourceType::Unknown  :
        case ResourceType::Count    : break;
    }

    Q_ASSERT(false);
    return {};
}
