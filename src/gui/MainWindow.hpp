#pragma once

#include <QMainWindow>
#include <cstdint>
#include "AppSettings.hpp"

// ----------------------------------------------------
// Forward Declarations cho các Widgets con (Best Practice)
// ----------------------------------------------------
class QTabWidget;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTextEdit;
class CppHighlighter;
class NotesAppCore;
class QShowEvent;
class QCloseEvent;
class QWidget;
class QComboBox;
class QLabel;
class TagInput;
class BrowseTabWidget;
class AddTabWidget;
class SettingsTabWidget;
class CodeEditorLineHighlighter;
class AppController;

// ----------------------------------------------------

// NOLINTNEXTLINE
enum class SettingsMessageState : std::uint8_t { None, Updated, Default };

class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override = default;

        void setAppController(AppController* controller);

        void applySettingsToUi(const AppSettings* settings);

        void retranslateUi();

        void applySyntaxHighlightingTheme(Theme theme);

    signals:
        void requestDatabaseInit(); // Gửi tín hiệu cho AppController/AppInitializer
        void requestDatabaseCreation();

    public slots:
        void setCore(NotesAppCore* core);
        void showError(const QString &message);
        void showInfo(const QString &message);

        void onDatabaseCreationRequested();

    protected:
        void showEvent(QShowEvent* event) override;
        void closeEvent(QCloseEvent* event) override;

        void changeEvent(QEvent* event) override;

    private slots:
        void onSearchClicked();
        void onAddNoteClicked();
        void onApplyButtonSettingsClicked();
        void onDefaultButtonSettingsClicked();
        void pickupFolder();

    private: // NOLINT(readability-redundant-access-specifiers)
        NotesAppCore* m_core{};
        AppController* m_appController{};

        // Widgets
        QTabWidget* m_tabWidget{};
        BrowseTabWidget* m_browseTab{};
        AddTabWidget* m_addTab{};
        SettingsTabWidget* m_settingsTab{};

        // Browse Tab

        // Add Tab
        CppHighlighter* m_cppHighlighter{};
        CodeEditorLineHighlighter* m_lineHighlighter{};

        // Settings Tab

        // Build UI internal
        void buildUi();
        void setupBrowseTab();
        void setupAddTab();
        void setupSettingsTab();

        void onTextRadioToggled(bool checked);

        void viewResource(const QString &id, const QString &title, const QString &path);

        void showContextMenu(const QPoint &pos, int row, const QString &id, const QString &title,
                             const QString &path);

        SettingsMessageState m_settingsMessageState{SettingsMessageState::None};
};
