#pragma once
#include <QString>
#include <cstdint>

namespace IpcNames {
    inline QString packerServer(std::int64_t resourceId) {
        return QStringLiteral("Notesman_Packer_%1").arg(resourceId);
    }

    constexpr auto K_GUI_SERVER = "Notesman_InstanceLock";
    constexpr int K_IPC_TIMEOUT_MS = 200;
} // namespace IpcNames
