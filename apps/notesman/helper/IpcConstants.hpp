#pragma once
#include <QString>

namespace IpcNames {

inline QString packerServer(char const* uuid) {
    return QStringLiteral("Notesman_Packer_%1").arg(uuid);
}

constexpr auto K_GUI_SERVER{"Notesman_InstanceLock"};
constexpr int K_IPC_TIMEOUT_MS{200};

} // namespace IpcNames
