#pragma once

#include <memory>
#include "NotesAppCore.hpp"
#include "AppSettings.hpp"

class QObject;
class QString;
class SQLiteDB;
class ResourceRepository;
class FileRepository;
class TextContentRepository;
class TagRepository;
class FileService;
class ResourceService;
class MainWindow;
class QTranslator;

class AppController : public QObject {
        Q_OBJECT

    public:
        explicit AppController(QObject* parent = nullptr);

        static void setupConnections(MainWindow &mainWindow, AppController &controller);

        void createDatabase();

        void verifyDatabase();

        void loadSettings();
        void saveSettings();

        void updateSettings(const AppSettings &newSettings);

        [[nodiscard]] const AppSettings* settings() const noexcept;

        void applyLanguage(Language lang);
        void applyTheme(Theme theme);

        void setMainWindow(MainWindow* window);

    signals:
        void coreReady(NotesAppCore* core);
        void errorOccurred(const QString &message);
        void infoMessage(const QString &message);

        void requestDatabaseCreation();

        void settingsLoaded(const AppSettings &settings);

        void languageChanged();

    public slots:
        void initializeCore();

    private:
        std::unique_ptr<NotesAppCore> m_core;

        std::unique_ptr<SQLiteDB> m_db;
        std::unique_ptr<ResourceRepository> m_resRepo;
        std::unique_ptr<FileRepository> m_fileRepo;
        std::unique_ptr<TextContentRepository> m_textRepo;
        std::unique_ptr<TagRepository> m_tagRepo;
        std::unique_ptr<FileService> m_fileService;
        std::unique_ptr<ResourceService> m_resService;

        std::unique_ptr<AppSettings> m_settings;

        std::unique_ptr<QTranslator> m_translator;

        MainWindow* m_mainWindow{nullptr};
};
