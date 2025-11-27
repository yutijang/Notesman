#pragma once

#include <QWidget>

#include "SettingsData.hpp"

class QRadioButton;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QHBoxLayout;

class SettingsTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit SettingsTabWidget(QWidget* parent = nullptr);
        ~SettingsTabWidget() override = default;

        void retranslateUi();

    public slots: // NOLINT(readability-redundant-access-specifiers)
        void showNotification(const QString &message);

        void handleInitialSettingsLoad(const SettingsData &settings) const;

        void handleSettingsStateChange(const SettingsData &settings) const;

        void handleUiRefreshRequest(const SettingsData &settings) const;

        void handleAfterLinkAccount(const QString &email);
        void handleAfterUnlinkAccount();

        void handleDownloadDBRequested(bool isDisabled);

    signals:
        // void applyClicked();
        void applySettingsRequested(const SettingsData &data);
        void defaultSettingsRequested();

        void requestGoogleLogin();
        void requestGoogleUnlink();

        void onUploadButtonClicked();
        void onDownloadButtonClicked();

    private slots:
        void onApplyBtnClicked();
        void onDefaultBtnClicked();
        void onBrowseBtnClicked();
        void loadSettingsToUi(const SettingsData &settings) const;

        void onLinkBtnClicked();

    private: // NOLINT(readability-redundant-access-specifiers)
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
        QPushButton* m_linkGDBtn{};
        QLabel* m_addressUserGMLoginLbl{};
        QPushButton* m_uploadDBBtn{};
        QPushButton* m_downloadDBBtn{};
        QLabel* m_notiSettingsChangedLbl{};

        bool m_isLinked{};

        void setupUi();
        void setupConnections();

        [[nodiscard]] QHBoxLayout* setupLanguageGroup();
        [[nodiscard]] QHBoxLayout* setupThemeGroup();
        [[nodiscard]] QHBoxLayout* setupResourceDirGroup();
        [[nodiscard]] QHBoxLayout* setupResourceManagerTypeGroup();
        [[nodiscard]] QHBoxLayout* setupButtonGroup();
        [[nodiscard]] QVBoxLayout* setupAccountLinkGroup();
};
