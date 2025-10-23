#pragma once

class QRadioButton;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

class SettingsTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit SettingsTabWidget(QWidget* parent = nullptr);
        ~SettingsTabWidget() override = default;

        void retranslateUi();

        // Getters cho MainWindow truy cập và đọc/ghi trạng thái
        [[nodiscard]] QRadioButton* langEnRad() const noexcept;
        [[nodiscard]] QRadioButton* langViRad() const noexcept;
        [[nodiscard]] QRadioButton* themeLightRad() const noexcept;
        [[nodiscard]] QRadioButton* themeDarkRad() const noexcept;
        [[nodiscard]] QLineEdit* resourceDirInput() const noexcept;
        [[nodiscard]] QComboBox* resourceManagementCombo() const noexcept;
        [[nodiscard]] QLabel* notificationLabel() const noexcept;
        [[nodiscard]] QPushButton* applyButton() const noexcept;
        [[nodiscard]] QPushButton* defaultButton() const noexcept;

    signals:
        // Tín hiệu cho MainWindow xử lý logic phức tạp (Apply, Default, Browse)
        void applyClicked();
        void defaultClicked();
        void resourceDirBrowseRequested();

    private slots:
        void onApplyBtnClicked();
        void onDefaultBtnClicked();
        void onBrowseBtnClicked();

    private: // NOLINT(readability-redundant-access-specifiers)
        void setupUi();
        void setupConnections();

        QRadioButton* m_langEnRad{};
        QRadioButton* m_langViRad{};
        QRadioButton* m_themeLightRad{};
        QRadioButton* m_themeDarkRad{};
        QLineEdit* m_resDirInp{};
        QPushButton* m_resDirBtn{};
        QComboBox* m_resManCom{};
        QPushButton* m_applyBtn{};
        QPushButton* m_defaultBtn{};
        QLabel* m_langLbl{};
        QLabel* m_themeLbl{};
        QLabel* m_resDirLbl{};
        QLabel* m_resManLbl{};
        QLabel* m_notiSettingsChangedLbl{};

        [[nodiscard]] QHBoxLayout* setupLanguageGroup();
        [[nodiscard]] QHBoxLayout* setupThemeGroup();
        [[nodiscard]] QHBoxLayout* setupResourceDirGroup();
        [[nodiscard]] QHBoxLayout* setupResourceManagerTypeGroup();
        [[nodiscard]] QHBoxLayout* setupButtonGroup();
};
