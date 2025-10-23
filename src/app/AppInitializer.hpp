#pragma once

#include <memory>

class AppController;
class MainWindow;

class AppInitializer {
    public:
        AppInitializer();
        ~AppInitializer();
        void run();

    private:
        std::unique_ptr<MainWindow> m_mainWindow;
        std::unique_ptr<AppController> m_controller;
};
