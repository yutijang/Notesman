#pragma once

#include <QWidget>
#include <optional>
#include <vector>

#include "model.hpp" // std::vector bắt buộc phải biết định nghĩa đầy đủ (tức là kích thước và cấu trúc) của kiểu dữ liệu mà nó chứa (FullResource) ngay tại thời điểm mẫu lớp std::vector được khởi tạo (instantiate) hoặc khai báo

class QLineEdit;
class QPushButton;
class QLabel;
class QRadioButton;
class ResultsTable;

class BrowseTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit BrowseTabWidget(QWidget* parent = nullptr);
        ~BrowseTabWidget() override = default;

        void retranslateUi();
        void displayResults(const std::vector<FullResource> &results);
        void updateColumnWidths();

        // Getter
        [[nodiscard]] ResultsTable* resultsTable() const noexcept { return m_resultsTbl; }

        void handleResultsSearchRequested(const std::vector<FullResource> &results);

    signals:
        void searchRequested(const QString &keyword, const QString &mode);
        void resourceDoubleClicked(int id, ResourceType type, const QString &title,
                                   const QString &path);
        void contextMenuRequested(const QPoint &pos, int id, ResourceType type,
                                  const QString &title, const QString &path);
        void statusUpdate(const QString &msg, int timeout);
        void getAllDataRequested();

    private slots:
        void onClearButtonClicked();
        void onGetAllButtonClicked();
        void onSearchButtonClicked();

    private: // NOLINT(readability-redundant-access-specifiers)
        struct RowData {
                int id;
                ResourceType type;
                QString title;
                QString path;
        };

        [[nodiscard]] std::optional<RowData> rowData(int row) const;

        void setupUI();
        void setupConnections();
        void onCellDoubleClicked(int row);
        void onCustomContextMenuRequested(const QPoint &pos);

        QLineEdit* m_searchInp{};
        QPushButton* m_searchBtn{};
        QLabel* m_searchByLbl{};
        QRadioButton* m_titleRad{};
        QRadioButton* m_contentRad{};
        QRadioButton* m_tagRad{};
        ResultsTable* m_resultsTbl{};

        // Group
        QPushButton* m_clearTableBtn{};
        QPushButton* m_getAllBtn{};
};
