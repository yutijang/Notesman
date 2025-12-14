#include "free_port.hpp"

#include <system_error>

// ---------------- Platform includes ----------------
#if defined(_WIN32)
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    include <stdexcept>

#    pragma comment(lib, "ws2_32.lib")

using SockHandle = SOCKET;
static constexpr SockHandle INVALID_SOCK = INVALID_SOCKET;

#else
#    include <sys/types.h>
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <unistd.h>

using SockHandle = int;
static constexpr SockHandle INVALID_SOCK = -1;
#endif

// ---------------- RAII: Socket ----------------
class Socket {
    public:
        explicit Socket(SockHandle fd) noexcept : m_fd{fd} {}

        ~Socket() {
            if (m_fd != INVALID_SOCK) {
#if defined(_WIN32)
                ::closesocket(m_fd);
#else
                ::close(m_fd);
#endif
            }
        }

        // Non-copy
        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;

        // Move
        Socket(Socket &&other) noexcept : m_fd{other.m_fd} { other.m_fd = INVALID_SOCK; }

        Socket &operator=(Socket &&other) noexcept {
            if (this != &other) {
                m_fd = other.m_fd;
                other.m_fd = INVALID_SOCK;
            }
            return *this;
        }

        [[nodiscard]] SockHandle get() const noexcept { return m_fd; }

    private:
        SockHandle m_fd;
};

// ---------------- RAII: Winsock (Windows only) ----------------
#if defined(_WIN32)
class WSA {
    public:
        WSA() {
            WSADATA wsa{};
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                throw std::runtime_error{"WSAStartup failed"};
            }
        }

        ~WSA() { WSACleanup(); }

        WSA(const WSA &) = delete;
        WSA &operator=(const WSA &) = delete;
};
#endif

// ---------------- Main function ----------------
std::uint16_t findFreePort() {
#if defined(_WIN32)
    WSA wsaGuard; // RAII init for Windows
#endif

    Socket sock{::socket(AF_INET, SOCK_STREAM, 0)};

#if defined(_WIN32)
    if (sock.get() == INVALID_SOCKET) {
        throw std::system_error{WSAGetLastError(), std::system_category(), "socket() failed"};
    }
#else
    if (sock.get() < 0) {
        throw std::system_error{errno, std::generic_category(), "socket() failed"};
    }
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0); // yêu cầu OS cấp port trống

    if (::bind(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#if defined(_WIN32)
        throw std::system_error{WSAGetLastError(), std::system_category(), "bind() failed"};
#else
        throw std::system_error{errno, std::generic_category(), "bind() failed"};
#endif
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(sock.get(), reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
#if defined(_WIN32)
        throw std::system_error{WSAGetLastError(), std::system_category(), "getsockname() failed"};
#else
        throw std::system_error{errno, std::generic_category(), "getsockname() failed"};
#endif
    }

    return ntohs(addr.sin_port);
}
