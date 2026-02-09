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
            const QString &message,
            UiConst::SettingsMessageState state = UiConst::SettingsMessageState::None,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleDeleteDBFileRespond(const QString &msg);
        void handleDBInfoGot(const QStringList &info);
        void handleLoginFailed(const QString &error = QString{});
        void handleDownloadDBRequested(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleUploadDBRequested(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void handleAfterUnlinkAccount();
        void handleAfterLinkAccount(const QString &htmlTextEmail);
        void handleInitialSettingsLoad(const SettingsData &settings) const;
        void handleSettingsStateChange(const SettingsData &settings) const;
        void handleUiRefreshRequest(const SettingsData &settings) const;

        void handleButtonAfterCleanup(UiConst::CleanupMode mode);

    Q_SIGNALS:
        void applySettingsRequested(const SettingsData &data);
        void defaultSettingsRequested();
        void requestGoogleLogin();
        void requestGoogleUnlink(bool isDeleteDB);
        void requestUpload();
        void requestDownload();
        void cancelLoginRequested();
        void statusUpdateRequest(const QString &msg, int timeout) const;

        void requestDBInfo();

        void cleanupEpubCacheNowRequest();
        void cleanupMDCacheNowRequest();

    private: // NOLINT(readability-redundant-access-specifiers)
        void setupUi();
        void setupConnections();
        void updateCountdownDisplay();
        void hideLoginStatus();
        void showLoginStatus();
        void validateResourceDir(const SettingsData &settings) const;

        void onApplyBtnClicked();
        void onDefaultBtnClicked();
        void onBrowseBtnClicked();
        void loadSettingsToUi(const SettingsData &settings) const;
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
