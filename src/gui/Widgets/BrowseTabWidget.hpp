#pragma once

#include <QWidget>
#include <optional>
#include <vector>
#include "model.hpp" // std::vector bắt buộc phải biết định nghĩa đầy đủ (tức là kích thước và cấu trúc) của kiểu dữ liệu mà nó chứa (FullResource) ngay tại thời điểm mẫu lớp std::vector được khởi tạo (instantiate) hoặc khai báo

class QWidget;
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
        [[nodiscard]] QString searchKeyword() const noexcept;
        [[nodiscard]] QRadioButton* titleRadio() const noexcept;
        [[nodiscard]] QRadioButton* contentRadio() const noexcept;
        [[nodiscard]] QRadioButton* tagRadio() const noexcept;
        [[nodiscard]] ResultsTable* resultsTable() const noexcept;

    signals:
        void searchRequested(const QString &keyword, const QString &mode);
        void resourceDoubleClicked(const QString &id, const QString &title, const QString &path);
        void contextMenuRequested(const QPoint &pos, int row, const QString &id,
                                  const QString &title, const QString &path);

    private:
        void setupUI();
        void setupConnections();
        void onCellDoubleClicked(int row);
        void onCustomContextMenuRequested(const QPoint &pos);

        struct RowData {
                QString id;
                QString title;
                QString path;
        };

        [[nodiscard]] std::optional<RowData> rowData(int row) const;

        QLineEdit* m_searchInp{};
        QPushButton* m_searchBtn{};
        QLabel* m_searchByLbl{};
        QRadioButton* m_titleRad{};
        QRadioButton* m_contentRad{};
        QRadioButton* m_tagRad{};
        ResultsTable* m_resultsTbl{};
};
