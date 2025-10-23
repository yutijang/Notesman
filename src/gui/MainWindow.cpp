#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QShowEvent>
#include <QScreen>
#include <QSettings>
#include <QDebug>
#include <QScrollBar>
#include <QFontDatabase>
#include <QTextEdit>
#include <QFileDialog>
#include <QMenu>
#include <QPoint>
#include <algorithm>
#include <ranges>

#include "UiConstants.hpp"
#include "BrowseTabWidget.hpp"
#include "AddTabWidget.hpp"
#include "SettingsTabWidget.hpp"
#include "MainWindow.hpp"
#include "AppSettings.hpp"
#include "CodeEditorLineHighlighter.hpp"
#include "ResultsTable.hpp"
#include "cpphighlightertheme.hpp"
#include "cpphighlighter.hpp"
#include "model.hpp"
#include "NotesAppCore.hpp"
#include "TagInput.hpp"
#include "AppController.hpp"

namespace {
    constexpr int GUI_WIDTH{1200};
    constexpr int GUI_HEIGHT{800};
    constexpr int NOTI_TIMEOUT{3000};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("Notes Manager"));
    resize(GUI_WIDTH, GUI_HEIGHT);

    buildUi();
}

void MainWindow::buildUi() {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    setupBrowseTab();
    setupAddTab();
    setupSettingsTab();

    m_tabWidget->setCurrentWidget(m_browseTab);
}

void MainWindow::setupBrowseTab() {
    m_browseTab = new BrowseTabWidget(this);

    connect(m_browseTab, &BrowseTabWidget::searchRequested, this, &MainWindow::onSearchClicked);

    connect(m_browseTab, &BrowseTabWidget::resourceDoubleClicked, this,
            [this](const QString &id, const QString &title, const QString &path) {
                viewResource(id, title, path);
            });

    connect(m_browseTab, &BrowseTabWidget::contextMenuRequested, this,
            &MainWindow::showContextMenu);

    m_tabWidget->addTab(m_browseTab, QIcon(":/icons/browse_tab.ico"), tr("Browse"));
}

void MainWindow::setupAddTab() {
    m_addTab = new AddTabWidget(this);

    connect(m_addTab, &AddTabWidget::addNoteRequested, this, &MainWindow::onAddNoteClicked);

    m_tabWidget->addTab(m_addTab, QIcon(":/icons/add_tab.ico"), tr("Add Note"));
}

void MainWindow::setupSettingsTab() {
    m_settingsTab = new SettingsTabWidget(this);

    // Kết nối các tín hiệu từ SettingsTabWidget đến các slot trong MainWindow
    connect(m_settingsTab, &SettingsTabWidget::applyClicked, this,
            &MainWindow::onApplyButtonSettingsClicked);
    connect(m_settingsTab, &SettingsTabWidget::defaultClicked, this,
            &MainWindow::onDefaultButtonSettingsClicked);

    // Kết nối tín hiệu browse folder đến hàm chung pickupFolder()
    connect(m_settingsTab, &SettingsTabWidget::resourceDirBrowseRequested, this,
            &MainWindow::pickupFolder);

    m_tabWidget->addTab(m_settingsTab, QIcon(":/icons/settings_tab.ico"), tr("Settings"));
}

void MainWindow::onSearchClicked() {
    if (m_core == nullptr) {
        showError(tr("Database not initialized."));
        return;
    }

    const QString keyword = m_browseTab->searchKeyword();
    if (keyword.isEmpty()) {
        showInfo(tr("Please enter a keyword to search."));
        return;
    }

    std::vector<FullResource> results;
    if (m_browseTab->titleRadio()->isChecked()) {
        results = m_core->searchByTitleFull(keyword.toUtf8().toStdString());
    } else if (m_browseTab->contentRadio()->isChecked()) {
        results = m_core->searchByContentFull(keyword.toUtf8().toStdString());
    } else if (m_browseTab->tagRadio()->isChecked()) {
        results = m_core->getFullResourcesByTag(keyword.toUtf8().toStdString());
    }
    if (results.empty()) { return; }

    m_browseTab->displayResults(results);

    m_browseTab->updateColumnWidths();
}

void MainWindow::onAddNoteClicked() {
    ResourceType type{};
    std::string pathStr;
    if (m_addTab->textRadio()->isChecked()) {
        type = ResourceType::text;
    } else if (m_addTab->fileRadio()->isChecked()) {
        const QString filePath = m_addTab->filePathInput()->text().trimmed();
        if (filePath.isEmpty()) { return; }

        pathStr = filePath.toUtf8().toStdString();
        if (m_core->isFileIndexed(pathStr)) {
            QMessageBox::information(this, tr("Information"),
                                     tr("File exists in storage! Not add more."));
            return;
        }

        auto typeOpt = resourceTypeFromFile(pathStr);
        if (typeOpt.has_value()) {
            type = *typeOpt;
        } else {
            QMessageBox::information(this, tr("Information"), tr("File extension not support!"));
            return;
        }
    }

    const QString title = m_addTab->titleInput()->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Title cannot be empty!"));
        return;
    }

    const std::string titleStd = title.toStdString();
    {
        bool isExistTitle = m_core->isExistTitle(titleStd, type);
        if (isExistTitle) {
            QMessageBox::information(this, tr("Information"),
                                     tr("Title exists! Please choose another title"));
            return;
        }
    }

    if (m_addTab->textRadio()->isChecked()) {
        const QString content = m_addTab->textEdit()->toPlainText();
        {
            const auto isEmptyContent = content.trimmed();
            if (isEmptyContent.isEmpty()) {
                QMessageBox::critical(this, tr("Error"), tr("Content cannot be empty!"));
                return;
            }
        }

        auto resId = m_core->addTextNote(titleStd, content.toUtf8().toStdString(), type);

        std::vector<std::string> tagNames;
        auto tags = m_addTab->tagInput()->getAllTags();
        if (!tags.isEmpty()) {
            std::ranges::transform(tags, std::back_inserter(tagNames),
                                   [](const QString &s) { return s.toStdString(); });
            m_core->addTags(resId, tagNames);
        }

        QMessageBox::information(this, tr("Add Note"), tr("Note added successfully!"));

        m_addTab->titleInput()->clear();
        m_addTab->tagInput()->clearTags();
        m_addTab->textEdit()->clear();

        return;
    }

    if (m_addTab->fileRadio()->isChecked()) {
        auto resId = m_core->addFileNote(pathStr, titleStd, type,
                                         m_appController->settings()->isManagedResources());

        std::vector<std::string> tagNames;
        auto tags = m_addTab->tagInput()->getAllTags();
        if (!tags.isEmpty()) {
            std::ranges::transform(tags, std::back_inserter(tagNames),
                                   [](const QString &s) { return s.toStdString(); });
            m_core->addTags(resId, tagNames);
        }

        QMessageBox::information(this, tr("Add File"), tr("File added successfully!"));

        m_addTab->titleInput()->clear();
        m_addTab->tagInput()->clearTags();
        m_addTab->textEdit()->clear();

        return;
    }
}

void MainWindow::onApplyButtonSettingsClicked() {
    if (m_core == nullptr) {
        QMessageBox::critical(this, tr("Error"), tr("Core is not initialized."));
        return;
    }

    const auto* constSettingsPtr = m_appController->settings();
    auto* settingsPtr = const_cast<AppSettings*>(constSettingsPtr);

    if (settingsPtr == nullptr) {
        QMessageBox::critical(this, tr("Error"), tr("Settings object is null."));
        return;
    }

    Language lang{};
    if (m_settingsTab->langEnRad()->isChecked()) {
        lang = Language::english;
    } else if (m_settingsTab->langViRad()->isChecked()) {
        lang = Language::vietnamese;
    }
    settingsPtr->setLanguage(lang);

    Theme theme{};
    if (m_settingsTab->themeLightRad()->isChecked()) {
        theme = Theme::light;
    } else if (m_settingsTab->themeDarkRad()->isChecked()) {
        theme = Theme::dark;
    }
    settingsPtr->setTheme(theme);

    {
        auto path = m_settingsTab->resourceDirInput()->text().trimmed();
        if (!path.isEmpty()) { settingsPtr->setResourceDir(path.toStdString()); }
    }

    {
        bool isMan = m_settingsTab->resourceManagementCombo()->currentData().toBool();
        settingsPtr->setManagedResources(isMan);
    }

    if (settingsPtr->isDirty()) {
        m_appController->saveSettings();

        m_appController->applyLanguage(lang);
        m_appController->applyTheme(theme);
        applySyntaxHighlightingTheme(settingsPtr->theme());

        m_settingsTab->notificationLabel()->setText(tr("Settings updated!"));
        m_settingsMessageState = SettingsMessageState::Updated;
    } else {
        m_settingsTab->notificationLabel()->setText(tr("Nothing changed, settings not save"));
        m_settingsMessageState = SettingsMessageState::None;
    }

    m_settingsTab->notificationLabel()->setVisible(true);

    QTimer::singleShot(NOTI_TIMEOUT, this, [this]() {
        m_settingsTab->notificationLabel()->clear();
        m_settingsTab->notificationLabel()->setVisible(false);
    });
}

void MainWindow::onDefaultButtonSettingsClicked() {
    const auto reply = QMessageBox::question(this, tr("Restore Defaults"),
                                             tr("Do you want to restore default settings?\nChanges "
                                                "will not be saved until you click Apply."),
                                             QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) { return; }

    const AppSettings defaultSettings{};
    applySettingsToUi(&defaultSettings);

    m_settingsTab->notificationLabel()->setText(tr("Settings default!"));
    m_settingsTab->notificationLabel()->setVisible(true);
    m_settingsMessageState = SettingsMessageState::Default;

    QTimer::singleShot(NOTI_TIMEOUT, this, [this]() {
        m_settingsTab->notificationLabel()->clear();
        m_settingsTab->notificationLabel()->setVisible(false);
    });
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);             // Gọi base trước

    QSettings settings("Notesman", "configs"); // Tạo INI/JSON config

    // Đọc vị trí lưu
    int x = settings.value("window/posX", -1).toInt();
    int y = settings.value("window/posY", -1).toInt();
    int w = settings.value("window/width", width()).toInt();
    int h = settings.value("window/height", height()).toInt();

    if (x != -1 && y != -1) {
        // Dùng vị trí lưu (và kích thước nếu cần)
        w = qMax(GUI_WIDTH, w);
        h = qMax(GUI_HEIGHT, h);
        resize(w, h);
        move(x, y);
    } else {
        // Fallback: Căn giữa màn hình
        QScreen* screen = QApplication::primaryScreen();
        QRect geom = screen->geometry();
        move((geom.width() - width()) / 2, (geom.height() - height()) / 2);
    }

    m_browseTab->updateColumnWidths();

    emit requestDatabaseInit();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Lưu vị trí và kích thước
    QSettings settings("Notesman", "configs");
    settings.setValue("window/posX", x());
    settings.setValue("window/posY", y());
    settings.setValue("window/width", width());
    settings.setValue("window/height", height());

    QMainWindow::closeEvent(event); // Gọi base để đóng
}

// ===================================================
// Core injection & helpers
// ===================================================
void MainWindow::setCore(NotesAppCore* core) {
    m_core = core;
    // showInfo(tr("Database initialized successfully."));

    m_tabWidget->setTabEnabled(m_tabWidget->indexOf(m_addTab), true);
    m_tabWidget->setTabEnabled(m_tabWidget->indexOf(m_browseTab), true);
}

void MainWindow::showError(const QString &message) {
    QMessageBox::critical(this, tr("Error"), message);
}

void MainWindow::showInfo(const QString &message) {
    QMessageBox::information(this, tr("Information"), message);
}

void MainWindow::onDatabaseCreationRequested() {
    const auto result = QMessageBox::question(
        this, tr("Database Missing"), tr("No database found. Would you like to create a new one?"),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) { emit requestDatabaseCreation(); }
}

void MainWindow::onTextRadioToggled(bool checked) {
    if (checked) {
        m_addTab->fileContainer()->hide();
        m_addTab->textEdit()->show();
    } else {
        m_addTab->fileContainer()->show();
        m_addTab->textEdit()->hide();
    }
}

void MainWindow::pickupFolder() {
    auto* senderButton = qobject_cast<QPushButton*>(sender());
    if (senderButton == nullptr) { return; }

    auto* targetEdit = senderButton->property("targetEdit").value<QLineEdit*>();
    if (targetEdit == nullptr) { return; }

    // API KHÁC: QFileDialog::getExistingDirectory, KHÔNG CÓ filter
    QString dirPath =
        QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), QDir::homePath());

    if (!dirPath.isEmpty()) { targetEdit->setText(QDir::toNativeSeparators(dirPath)); }
}

// NOLINTNEXTLINE
void MainWindow::viewResource(const QString &id, const QString &title, const QString &path) {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose); // Tự giải phóng khi đóng
    dialog->setWindowTitle(QString("Chi tiết tài liệu: %1").arg(title));

    const int editorW = m_addTab->editorWidth();
    const int mainH = this->height();
    dialog->resize(editorW, mainH);

    auto* viewSourceTextEdit = new QTextEdit(dialog);
    viewSourceTextEdit->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                                Qt::TextSelectableByKeyboard | Qt::TextEditable);
    viewSourceTextEdit->setFont(QFont("JetBrains Mono", UiConst::FONT_SIZE));
    viewSourceTextEdit->setFixedWidth(editorW);

    Theme curTheme = m_appController->settings()->theme();
    const CppHighlighterTheme hlTheme =
        (curTheme == Theme::light) ? createLightTheme() : createDarkTheme();
    auto* cppHighlighter = new CppHighlighter(viewSourceTextEdit->document(), hlTheme);
    cppHighlighter->rehighlightGradually(viewSourceTextEdit->document(),
                                         20, // NOLINT(readability-magic-numbers)
                                         4);

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(viewSourceTextEdit);
    dialog->setLayout(layout);

    if (path.isEmpty()) {
        bool ok = false;
        const sqlite3_int64 resId = id.toLongLong(&ok);
        if (ok) {
            auto resFullOpt = m_core->getFullResource(resId);
            if (resFullOpt && resFullOpt->content) {
                viewSourceTextEdit->setPlainText(QString::fromStdString(*resFullOpt->content));
            } else {
                viewSourceTextEdit->setPlainText(tr("No content available."));
            }
        }
    } else {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            viewSourceTextEdit->setPlainText(in.readAll());
            file.close();
        } else {
            qDebug() << "Error: Cannot open file" << path;
            viewSourceTextEdit->setPlainText(tr("Error: Cannot load file '%1'").arg(path));
        }
    }

    dialog->show();

    QTimer::singleShot(0, this, [this]() {
        m_browseTab->resultsTable()->clearSelection();
        m_browseTab->resultsTable()->setCurrentItem(nullptr);
        m_browseTab->resultsTable()->clearFocus();
    });
}

void MainWindow::showContextMenu(const QPoint &pos, int row, const QString &id,
                                 const QString &title, const QString &path) {
    if (m_browseTab == nullptr) {
        qWarning() << "BrowseTabWidget not initialized!";
        return;
    }

    auto* resultsTbl = m_browseTab->resultsTable();
    if (resultsTbl == nullptr) {
        qWarning() << "ResultsTable not available!";
        return;
    }

    QMenu menu(this);

    QAction* viewAction{};
    // if (path.isEmpty()) {
    viewAction = menu.addAction("View Resource");
    viewAction->setIcon(QIcon(":/icons/view.ico"));
    connect(viewAction, &QAction::triggered, this,
            [this, id, title, path]() { viewResource(id, title, path); });
    // }

    QAction* openAction = menu.addAction("Open path");
    connect(openAction, &QAction::triggered, this, [path]() {
        if (path.isEmpty()) {
            qDebug() << "No path to open.";
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    menu.addSeparator();

    QAction* deleteAction = menu.addAction("Delete Resource");
    deleteAction->setIcon(QIcon(":/icons/erase.ico"));
    connect(deleteAction, &QAction::triggered, this, [this, &resultsTbl] {
        const auto selectedRows = resultsTbl->selectionModel()->selectedRows();
        if (selectedRows.empty()) { return; }

        QVector<sqlite3_int64> idsToDelete;
        idsToDelete.reserve(selectedRows.size());

        for (const QModelIndex &idx : selectedRows) {
            const QString id = resultsTbl->item(idx.row(), 0)->text();
            bool ok = false;
            const sqlite3_int64 resId = id.toLongLong(&ok);
            if (ok) { idsToDelete.append(resId); }
        }

        for (const auto &selectedRow : std::ranges::reverse_view(selectedRows)) {
            resultsTbl->removeRow(selectedRow.row());
        }

        for (sqlite3_int64 id : idsToDelete) { m_core->deleteResource(id); }
    });

    menu.exec(resultsTbl->viewport()->mapToGlobal(
        pos + QPoint(5, 5))); // NOLINT(readability-magic-numbers)
}

void MainWindow::applySettingsToUi(const AppSettings* settings) {
    if (settings == nullptr) { return; }

    // Ngôn ngữ
    if (settings->language() == Language::english) {
        m_settingsTab->langEnRad()->setChecked(true);
    } else if (settings->language() == Language::vietnamese) {
        m_settingsTab->langViRad()->setChecked(true);
    }

    // Giao diện
    if (settings->theme() == Theme::light) {
        m_settingsTab->themeLightRad()->setChecked(true);
    } else if (settings->theme() == Theme::dark) {
        m_settingsTab->themeDarkRad()->setChecked(true);
    }

    // Thư mục tài nguyên
    m_settingsTab->resourceDirInput()->setText(QString::fromStdString(settings->resourceDir()));

    // Kiểu quản lý tài nguyên
    if (settings->isManagedResources()) {
        m_settingsTab->resourceManagementCombo()->setCurrentIndex(0); // "Notes Manager"
    } else {
        m_settingsTab->resourceManagementCombo()->setCurrentIndex(1); // "Save path only"
    }
}

void MainWindow::setAppController(AppController* controller) {
    m_appController = controller;

    connect(m_appController, &AppController::settingsLoaded, this,
            [this](const AppSettings &settings) {
                if (m_tabWidget->currentIndex() == 2) { applySettingsToUi(&settings); }
            });

    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        constexpr int settingsTabIndex = 2;
        if (index == settingsTabIndex && m_appController) {
            applySettingsToUi(m_appController->settings());
        }
    });
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) { retranslateUi(); }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("Notes Manager"));

    m_tabWidget->setTabText(0, tr("Browse"));
    m_tabWidget->setTabText(1, tr("Add"));
    m_tabWidget->setTabText(2, tr("Settings"));

    // ========= Browse Tab =========
    m_browseTab->retranslateUi();
    // ==============================

    // ========= Add Tab =========
    m_addTab->retranslateUi();
    // ===========================

    // ========= Settings Tab =========
    m_settingsTab->retranslateUi();

    // Dịch lại thông báo đang hiển thị (nếu còn hiệu lực)
    switch (m_settingsMessageState) {
        case SettingsMessageState::Updated:
            m_settingsTab->notificationLabel()->setText(tr("Settings updated!"));
            break;
        case SettingsMessageState::Default:
            m_settingsTab->notificationLabel()->setText(tr("Settings default!"));
            break;
        case SettingsMessageState::None:
        default                        : break;
    }
    m_settingsTab->applyButton()->setText(tr("Apply"));
    m_settingsTab->defaultButton()->setText(tr("Default"));

    // =================================
}

void MainWindow::applySyntaxHighlightingTheme(Theme theme) {
    if (m_addTab->textEdit() == nullptr) { return; }

    // Chọn theme tô màu
    const CppHighlighterTheme hlTheme =
        (theme == Theme::light) ? createLightTheme() : createDarkTheme();

    // Nếu chưa có highlighter thì tạo mới
    if (m_cppHighlighter == nullptr) {
        m_cppHighlighter = new CppHighlighter(m_addTab->textEdit()->document(), hlTheme);
    } else {
        m_cppHighlighter->stopGradualRehighlight();
        m_cppHighlighter->setTheme(hlTheme);
    }

    // Áp dụng highlight dần (mượt, không đơ)
    m_cppHighlighter->rehighlightGradually(m_addTab->textEdit()->document(),
                                           20, // NOLINT(readability-magic-numbers)
                                           4);

    // Cập nhật dòng caret highlight
    if (m_lineHighlighter != nullptr) {
        delete m_lineHighlighter;
        m_lineHighlighter = nullptr;
    }
    m_lineHighlighter = new CodeEditorLineHighlighter(m_addTab->textEdit());
    if (theme == Theme::light) {
        m_lineHighlighter->setColors(QColor("#dBdBdB"), QColor("#efefef"));
    } else {
        m_lineHighlighter->setColors(QColor("#2f2f2f"), QColor("#2a2a2a"));
    }
}
