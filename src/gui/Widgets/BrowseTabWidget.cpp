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

#include "BrowseTabWidget.hpp"
#include "ResultsTable.hpp"
#include "model.hpp"
#include "UiConstants.hpp"

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
    searchGroupLayout->addLayout(searchInpLayout);

    auto* radioLayout = new QHBoxLayout();
    radioLayout->addStretch(1);
    m_searchByLbl = new QLabel(tr("Search by: "));
    m_titleRad = new QRadioButton(tr("Title"));
    m_contentRad = new QRadioButton(tr("Content"));
    m_tagRad = new QRadioButton("Tag");
    m_titleRad->setChecked(true);
    auto* searchByGroup = new QButtonGroup(this);
    searchByGroup->addButton(m_titleRad);
    searchByGroup->addButton(m_contentRad);
    searchByGroup->addButton(m_tagRad);
    radioLayout->addWidget(m_searchByLbl);
    radioLayout->addWidget(m_titleRad);
    radioLayout->addWidget(m_contentRad);
    radioLayout->addWidget(m_tagRad);
    radioLayout->setSpacing(20); // NOLINT(readability-magic-numbers)
    radioLayout->addStretch(1);
    searchGroupLayout->addLayout(radioLayout);
    searchGroupLayout->addStretch(1);

    // Group ======
    auto* utilityGroupLayout = new QVBoxLayout();
    auto* utilityBox = new QGroupBox();
    utilityBox->setMinimumSize(0, 108);               // NOLINT(readability-magic-numbers)

    auto* utilityBoxLayout = new QVBoxLayout(utilityBox);
    utilityBoxLayout->setSpacing(5);                  // NOLINT(readability-magic-numbers)
    utilityBoxLayout->setContentsMargins(5, 5, 5, 5); // NOLINT(readability-magic-numbers)

    m_clearTableBtn = new QPushButton(tr("Clear"));
    m_clearTableBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    utilityBoxLayout->addWidget(m_clearTableBtn, 0, Qt::AlignCenter);
    m_getAllBtn = new QPushButton(tr("Get All"));
    m_getAllBtn->setMinimumWidth(UiConst::BUTTON_WIDTH);
    utilityBoxLayout->addWidget(m_getAllBtn, 0, Qt::AlignCenter);
    utilityBoxLayout->addStretch(1);

    utilityGroupLayout->addWidget(utilityBox);
    utilityGroupLayout->addStretch(1);
    // Group ======

    topLayout->addLayout(searchGroupLayout, 2);
    topLayout->addLayout(utilityGroupLayout, 3);

    m_resultsTbl = new ResultsTable(this);
    m_resultsTbl->setContextMenuPolicy(Qt::CustomContextMenu);
    m_resultsTbl->setColumnCount(3);
    m_resultsTbl->setHorizontalHeaderLabels({tr("ID"), tr("Title"), tr("Path")});
    m_resultsTbl->horizontalHeader()->setStretchLastSection(true);
    m_resultsTbl->verticalHeader()->setVisible(false);
    m_resultsTbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_resultsTbl->setColumnWidth(0, 50); // NOLINT(readability-magic-numbers)
    m_resultsTbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTbl->setSortingEnabled(true);

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

    QObject::connect(m_getAllBtn, &QPushButton::clicked, this,
                     &BrowseTabWidget::getAllDataRequested);
}

void BrowseTabWidget::retranslateUi() {
    m_searchBtn->setText(tr("Search"));
    m_searchInp->setPlaceholderText(tr("Enter keyword..."));
    m_searchByLbl->setText(tr("Search by: "));
    m_titleRad->setText(tr("Title"));
    m_contentRad->setText(tr("Content"));

    m_resultsTbl->setHorizontalHeaderLabels({tr("ID"), tr("Title"), tr("Path")});
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

// signals custom
// NOLINTNEXTLINE
void BrowseTabWidget::onCellDoubleClicked(int row) {
    auto rowDataOpt = rowData(row);
    if (!rowDataOpt.has_value()) {
        qWarning() << "Invalid row" << row;
        return;
    }

    const auto &data = *rowDataOpt;

    emit resourceDoubleClicked(data.id, data.title, data.path);
}

void BrowseTabWidget::displayResults(const std::vector<FullResource> &results) {
    if (results.empty()) { return; }

    m_resultsTbl->setRowCount(0); // Dọn dẹp (clear) hoặc chuẩn bị lại bảng kết quả

    m_resultsTbl->setUpdatesEnabled(false);
    m_resultsTbl->setSortingEnabled(false);

    // for (const auto &res : results) {
    //     const int row = m_resultsTbl->rowCount();
    //     m_resultsTbl->insertRow(row);

    //     auto* idItem = new QTableWidgetItem(QString::number(res.resource.id));
    //     idItem->setTextAlignment(Qt::AlignCenter);
    //     idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    //     m_resultsTbl->setItem(row, 0, idItem);

    //     m_resultsTbl->setItem(row, 1,
    //                           new QTableWidgetItem(QString::fromStdString(res.resource.title)));

    //     if (res.filepath.has_value()) {
    //         m_resultsTbl->setItem(row, 2,
    //                               new QTableWidgetItem(QString::fromStdString(*res.filepath)));
    //     }
    // }

    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto &res = results[i];
        const int row = m_resultsTbl->rowCount();
        m_resultsTbl->insertRow(row);

        auto* noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        noItem->setFlags(noItem->flags() & ~Qt::ItemIsEditable);
        m_resultsTbl->setItem(row, 0, noItem);

        auto* titleItem = new QTableWidgetItem(QString::fromStdString(res.resource.title));
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);

        titleItem->setData(Qt::UserRole, res.resource.id);

        m_resultsTbl->setItem(row, 1, titleItem);

        if (res.filepath.has_value()) {
            auto* pathItem = new QTableWidgetItem(QString::fromStdString(*res.filepath));
            pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
            m_resultsTbl->setItem(row, 2, pathItem);
        }
    }

    m_resultsTbl->setUpdatesEnabled(true);
    m_resultsTbl->setSortingEnabled(true);

    emit statusUpdate(tr("Found %1 results").arg(QString::number(results.size())),
                      UiConst::NOTI_TIMEOUT);
}

void BrowseTabWidget::onCustomContextMenuRequested(const QPoint &pos) {
    const QModelIndex &index = m_resultsTbl->indexAt(pos);
    if (!index.isValid()) { return; }

    const int &row = index.row();

    auto* titleItem = m_resultsTbl->item(row, 1);
    if (titleItem == nullptr) { return; }

    QVariant idData = titleItem->data(Qt::UserRole);
    if (!idData.isValid()) { return; }
    const int &idItem = idData.toInt();

    auto* pathItem = m_resultsTbl->item(row, 2);
    const QString &path = (pathItem != nullptr) ? pathItem->text() : QString();

    emit contextMenuRequested(pos, idItem, titleItem->text(), path);
}

std::optional<BrowseTabWidget::RowData> BrowseTabWidget::rowData(int row) const {
    if (row < 0 || row >= m_resultsTbl->rowCount()) { return std::nullopt; }

    auto* titleItem = m_resultsTbl->item(row, 1);
    if (titleItem == nullptr) { return std::nullopt; }

    QVariant idData = titleItem->data(Qt::UserRole);
    if (!idData.isValid()) { return std::nullopt; }

    auto* pathItem = m_resultsTbl->item(row, 2);

    RowData r{.id = idData.toInt(),
              .title = titleItem->text(),
              .path = (pathItem != nullptr) ? pathItem->text() : QString()};

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
    emit getAllDataRequested();
}

void BrowseTabWidget::onSearchButtonClicked() {
    // IIFE: Biểu thức lambda được định nghĩa và gọi ngay lập tức ()
    const QString mode = [this]() -> QString {
        if (m_titleRad->isChecked()) { return "title"; }
        if (m_contentRad->isChecked()) { return "content"; }
        // Luôn phải có return cuối cùng cho các trường hợp còn lại
        return "tag";
    }(); // Dấu ngoặc () ở cuối để gọi lambda ngay lập tức

    emit searchRequested(m_searchInp->text().trimmed(), mode);
}

void BrowseTabWidget::handleResultsSearchRequested(const std::vector<FullResource> &results) {
    displayResults(results);
    updateColumnWidths();
}
