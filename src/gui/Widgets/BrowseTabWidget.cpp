#include "BrowseTabWidget.hpp"
#include "ResultsTable.hpp"
#include "model.hpp"

BrowseTabWidget::BrowseTabWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
    setupConnections();
}

void BrowseTabWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* searchLayout = new QHBoxLayout();
    m_searchInp = new QLineEdit();
    m_searchInp->setPlaceholderText(tr("Enter keyword..."));

    m_searchBtn = new QPushButton(tr("Search"));
    m_searchBtn->setIcon(QIcon(":/icons/search_button.ico"));

    searchLayout->addWidget(m_searchInp);
    searchLayout->addWidget(m_searchBtn);
    searchLayout->setSpacing(3);

    auto* filterContainer = new QWidget(this);
    auto* filterLayout = new QHBoxLayout(filterContainer);
    filterLayout->setContentsMargins(0, 5, 0, 20); // NOLINT(readability-magic-numbers)
    filterLayout->setSpacing(20);                  // NOLINT(readability-magic-numbers)

    m_searchByLbl = new QLabel(tr("Search by: "));
    m_titleRad = new QRadioButton(tr("Title"));
    m_contentRad = new QRadioButton(tr("Content"));
    m_tagRad = new QRadioButton("Tag");
    m_titleRad->setChecked(true);

    auto* searchByGroup = new QButtonGroup(this);
    searchByGroup->addButton(m_titleRad);
    searchByGroup->addButton(m_contentRad);
    searchByGroup->addButton(m_tagRad);

    filterLayout->addWidget(m_searchByLbl);
    filterLayout->addWidget(m_titleRad);
    filterLayout->addWidget(m_contentRad);
    filterLayout->addWidget(m_tagRad);

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

    mainLayout->addLayout(searchLayout);
    mainLayout->addWidget(filterContainer, 0, Qt::AlignHCenter);
    mainLayout->addWidget(m_resultsTbl);
}

void BrowseTabWidget::setupConnections() {
    connect(m_searchBtn, &QPushButton::clicked, this, [this]() {
        // IIFE: Biểu thức lambda được định nghĩa và gọi ngay lập tức ()
        const QString mode = [this]() -> QString {
            if (m_titleRad->isChecked()) { return "title"; }
            if (m_contentRad->isChecked()) { return "content"; }
            // Luôn phải có return cuối cùng cho các trường hợp còn lại
            return "tag";
        }(); // Dấu ngoặc () ở cuối để gọi lambda ngay lập tức

        emit searchRequested(m_searchInp->text(), mode);
    });

    connect(m_searchInp, &QLineEdit::returnPressed, m_searchBtn, &QPushButton::click);

    connect(m_resultsTbl, &QTableWidget::cellDoubleClicked, this,
            &BrowseTabWidget::onCellDoubleClicked);

    connect(m_resultsTbl, &QWidget::customContextMenuRequested, this,
            &BrowseTabWidget::onCustomContextMenuRequested);
}

void BrowseTabWidget::retranslateUi() {
    m_searchBtn->setText(tr("Search"));
    m_searchInp->setPlaceholderText(tr("Enter keyword..."));
    m_searchByLbl->setText(tr("Search by: "));
    m_titleRad->setText(tr("Title"));
    m_contentRad->setText(tr("Content"));

    m_resultsTbl->setHorizontalHeaderLabels({tr("ID"), tr("Title"), tr("Path")});
}

QString BrowseTabWidget::searchKeyword() const noexcept {
    return m_searchInp->text().trimmed();
}

QRadioButton* BrowseTabWidget::titleRadio() const noexcept {
    return m_titleRad;
}

QRadioButton* BrowseTabWidget::contentRadio() const noexcept {
    return m_contentRad;
}

QRadioButton* BrowseTabWidget::tagRadio() const noexcept {
    return m_tagRad;
}

ResultsTable* BrowseTabWidget::resultsTable() const noexcept {
    return m_resultsTbl;
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
    m_resultsTbl->setRowCount(0); // Dọn dẹp (clear) hoặc chuẩn bị lại bảng kết quả

    for (const auto &res : results) {
        const int row = m_resultsTbl->rowCount();
        m_resultsTbl->insertRow(row);

        auto* idItem = new QTableWidgetItem(QString::number(res.resource.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        m_resultsTbl->setItem(row, 0, idItem);

        m_resultsTbl->setItem(row, 1,
                              new QTableWidgetItem(QString::fromStdString(res.resource.title)));

        if (res.filepath.has_value()) {
            m_resultsTbl->setItem(row, 2,
                                  new QTableWidgetItem(QString::fromStdString(*res.filepath)));
        }
    }
}

void BrowseTabWidget::onCustomContextMenuRequested(const QPoint &pos) {
    const QModelIndex index = m_resultsTbl->indexAt(pos);
    if (!index.isValid()) { return; }

    const int row = index.row();

    auto* idItem = m_resultsTbl->item(row, 0);
    auto* titleItem = m_resultsTbl->item(row, 1);
    auto* pathItem = m_resultsTbl->item(row, 2);

    if ((idItem == nullptr) || (titleItem == nullptr)) {
        return; // Tránh dereference null pointer
    }

    const QString &id = idItem->text();
    const QString &title = titleItem->text();
    const QString &path = (pathItem != nullptr) ? pathItem->text() : QString();

    emit contextMenuRequested(pos, row, id, title, path);
}

std::optional<BrowseTabWidget::RowData> BrowseTabWidget::rowData(int row) const {
    if (row < 0 || row >= m_resultsTbl->rowCount()) { return std::nullopt; }

    auto* idItem = m_resultsTbl->item(row, 0);
    auto* titleItem = m_resultsTbl->item(row, 1);
    auto* pathItem = m_resultsTbl->item(row, 2);

    if ((idItem == nullptr) || (titleItem == nullptr)) { return std::nullopt; }

    RowData r{.id = idItem->text(),
              .title = titleItem->text(),
              .path = (pathItem != nullptr) ? pathItem->text() : QString()};

    return r;
}
