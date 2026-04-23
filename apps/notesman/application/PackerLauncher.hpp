#pragma once

#include <QString>

class PackerLauncher {
    public:
        PackerLauncher() = delete;

        // Entry point cho packer mode — gọi từ main.cpp khi có --open-packer
        // Trả về exit code: 0 = thành công, 1 = lỗi
        static int run(QString const& packerFilePath);
};
