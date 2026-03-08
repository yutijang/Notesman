#pragma once

#include "model.hpp" // std::vector bắt buộc phải biết định nghĩa đầy đủ (tức là kích thước và cấu trúc) của kiểu dữ liệu mà nó chứa (FullResource) ngay tại thời điểm mẫu lớp std::vector được khởi tạo (instantiate) hoặc khai báo

#include <QObject>
#include <QString>
#include <QWidget>
#include <optional>
#include <vector>

class QLineEdit;
class QPushButton;
class QLabel;
class QRadioButton;
class ResultsTable;
class QComboBox;
class QHBoxLayout;

class BrowseTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit BrowseTabWidget(QWidget* parent = nullptr);
        ~BrowseTabWidget() override = default;

        void retranslateUi();
        void displayResults(std::vector<UnifiedSearchResult> const& results);
        void updateColumnWidths();

        // Getter
        [[nodiscard]] ResultsTable* resultsTable() const noexcept { return m_resultsTbl; }

        void handleResultsSearchRequested(std::vector<UnifiedSearchResult> const& results);

    Q_SIGNALS:
        void searchRequested(QString const& keyword, QString const& mode);
        void resourceDoubleClicked(int id, ResourceType type, QString const& title,
                                   QString const& path, QString const& url);
        void contextMenuRequested(QPoint const& pos, int id, ResourceType type,
                                  QString const& title, QString const& path, QString const& url);
        void statusUpdateRequest(QString const& msg, int timeout);
        void loadAllDataRequested();

        void loadResourceByTypeRequested(ResourceType type);

    private: // NOLINT(readability-redundant-access-specifiers)
        struct RowData {
                int id;
                ResourceType type;
                QString title;
                QString path;
                QString url;
        };

        [[nodiscard]] std::optional<RowData> rowData(int row) const;

        void setupUI();
        void setupConnections();
        void onCellDoubleClicked(int row);
        void onCustomContextMenuRequested(QPoint const& pos);

        static ResourceType currentResourceType(QComboBox const* combo);
        static void populateResourceTypeCombo(QComboBox* combo);

        static QString resourceTypeToDisplayText(ResourceType type);

        // Group search layout
        QHBoxLayout* setupSearchInpLayoutGroup();
        QHBoxLayout* setupRadioLayoutGroup();

        // Group utility
        QWidget* setuputilityContainerGroup();

        // Group result table
        void setupResultTableGroup();

        void onClearButtonClicked();
        void onGetAllButtonClicked();
        void onSearchButtonClicked();

        QLineEdit* m_searchInp{};
        QPushButton* m_searchBtn{};
        QLabel* m_searchByLbl{};
        QRadioButton* m_titleRad{};
        QRadioButton* m_contentRad{};
        QRadioButton* m_tagRad{};
        QRadioButton* m_allRad{};
        ResultsTable* m_resultsTbl{};
        QComboBox* m_getResTypeCom{};
        QPushButton* m_loadResTypeBtn{};

        // Group
        QPushButton* m_clearTableBtn{};
        QPushButton* m_loadAllBtn{};
};
