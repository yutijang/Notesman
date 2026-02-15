#pragma once

#include <QWidget>
#include <QTimer>
#include <QString>
#include <QObject>
#include <QStringList>

#include "SettingsData.hpp"
#include "UiConstants.hpp"

class QRadioButton;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QGroupBox;
class QCheckBox;
class QSpinBox;

class SettingsTabWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit SettingsTabWidget(QWidget* parent = nullptr);
        ~SettingsTabWidget() override = default;

        void retranslateUi();

        void showNotification(
            QString const& message,
            UiConst::SettingsMessageState state = UiConst::SettingsMessageState::None,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleDeleteDBFileRespond(QString const& msg);
        void handleDBInfoGot(QStringList const& info);
        void handleLoginFailed(QString const& error = QString{});
        void handleDownloadDBRequested(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleUploadDBRequested(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleAfterUnlinkAccount();
        void handleAfterLinkAccount(QString const& htmlTextEmail);
        void handleInitialSettingsLoad(SettingsData const& settings) const;
        void handleSettingsStateChange(SettingsData const& settings) const;
        void handleUiRefreshRequest(SettingsData const& settings) const;

        void handleButtonAfterCleanup(UiConst::CleanupMode mode);

    Q_SIGNALS:
        void applySettingsRequested(SettingsData const& data);
        void defaultSettingsRequested();
        void requestGoogleLogin();
        void requestGoogleUnlink(bool isDeleteDB);
        void requestUpload();
        void requestDownload();
        void cancelLoginRequested();
        void statusUpdateRequest(QString const& msg, int timeout) const;

        void requestDBInfo();

        void cleanupEpubCacheNowRequest();
        void cleanupMDCacheNowRequest();

    private: // NOLINT(readability-redundant-access-specifiers)
        void setupUi();
        void setupConnections();
        void updateCountdownDisplay();
        void hideLoginStatus();
        void showLoginStatus();
        void validateResourceDir(SettingsData const& settings) const;

        void onApplyBtnClicked();
        void onDefaultBtnClicked();
        void onBrowseBtnClicked();
        void loadSettingsToUi(SettingsData const& settings) const;
        void onLinkBtnClicked();
        void onUploadButtonClicked();
        void onDownloadButtonClicked();

        [[nodiscard]] QHBoxLayout* setupLanguageGroup();
        [[nodiscard]] QHBoxLayout* setupThemeGroup();
        [[nodiscard]] QHBoxLayout* setupResourceDirGroup();
        [[nodiscard]] QHBoxLayout* setupResourceManagerTypeGroup();
        [[nodiscard]] QHBoxLayout* setupButtonGroup();
        [[nodiscard]] QVBoxLayout* setupAccountLinkGroup();
        [[nodiscard]] QWidget* setupLoginStatusGroup();
        [[nodiscard]] QGroupBox* setupCleanupGroup();

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
        QPushButton* m_checkRemoteDBInfoBtn{};

        bool m_isLinked{};

        // Cleanup cache EPUB + Markdown
        QGroupBox* m_cleanupCacheGBox{};
        QLabel* m_cleanupHtmlFromMDCacheLbl{};
        QSpinBox* m_expiredEpubSpbx{};
        QSpinBox* m_expiredMDSpbx{};
        QPushButton* m_cleanupEpubCacheNowBtn{};
        QPushButton* m_cleanupMDCacheNowBtn{};
        QCheckBox* m_cleanupEpubAfterChk{};
        QCheckBox* m_cleanupMDAfterChk{};

        QWidget* m_loginStatusWidget{};
        QLabel* m_statusLabel{};
        QLabel* m_countdownLabel{};
        QPushButton* m_cancelLoginBtn{};
        QTimer m_countdownTimer;
        int m_remainingSeconds{};
        QLabel* m_info1{};
        QLabel* m_info2{};
        QLabel* m_info3{};
        QLabel* m_info4{};
};
