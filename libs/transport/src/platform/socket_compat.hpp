#pragma once

// transport 的 socket 平台兼容薄层（P2 收编）。
//
// 独立库自带 platform 层（transport 需独立移植到 Android，不依赖
// lib/wingman 的 src/platform/）：统一 Winsock / POSIX socket 的
// 头卫生、句柄类型、错误码与收发原语，让 transport 的公共头与
// 实现保持零平台条件编译。
// 规范：docs/platform-abstraction-design.md §8.4「独立库薄层」。

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h> // tcp_keepalive / SIO_KEEPALIVE_VALS
#include <cstddef>

// Windows.h 的宏污染（GetMessage/min/max 等）会撞掉方法名，
// 在包含 Winsock 的同时一并清除
#ifdef Get
#undef Get
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace wingman::transport {

using SocketType = SOCKET;

// Windows 无 POSIX 的 ssize_t（MSVC 仅在 CRT 宏分歧下偶发提供），
// 与 POSIX 侧同语义：收发原语的返回类型（<0 错误，0 对端关闭）
using ssize_t = std::ptrdiff_t;

#ifndef INVALID_SOCKET_VALUE
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#endif
#ifndef SOCKET_ERROR_VALUE
#define SOCKET_ERROR_VALUE SOCKET_ERROR
#endif

// 进程内一次性 Winsock 初始化（POSIX 下为 no-op）
inline void ensureWinsock() {
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            initialized = true;
        }
    }
}

// 统一关闭：先 shutdown 唤醒阻塞 recv()（见 StreamChannel::disconnect 注释），
// 再释放句柄
inline void closeSocketCompat(SocketType socket) {
    shutdown(socket, SD_BOTH);
    closesocket(socket);
}

// 收发原语：统一为 ssize_t 语义（<0 错误，0 对端关闭）
inline ssize_t sendCompat(SocketType socket, const void* data, size_t size) {
    return ::send(socket, static_cast<const char*>(data),
                  static_cast<int>(size), 0);
}

inline ssize_t recvCompat(SocketType socket, void* buffer, size_t size) {
    return ::recv(socket, static_cast<char*>(buffer),
                  static_cast<int>(size), 0);
}

inline int lastSocketError() {
    return WSAGetLastError();
}

inline bool isWouldBlock(int error) {
    return error == WSAEWOULDBLOCK;
}

// SO_KEEPALIVE + 参数（尽力而为）
inline void setTcpKeepAlive(SocketType socket, int idleSecs, int intervalSecs, int /*count*/) {
    tcp_keepalive keepalive{};
    keepalive.onoff = 1;
    keepalive.keepalivetime = static_cast<ULONG>(idleSecs) * 1000;
    keepalive.keepaliveinterval = static_cast<ULONG>(intervalSecs) * 1000;
    DWORD bytesReturned = 0;
    WSAIoctl(socket, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
             nullptr, 0, &bytesReturned, nullptr, nullptr);
}

} // namespace wingman::transport

#else // POSIX

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cerrno>

namespace wingman::transport {

using SocketType = int;

#ifndef INVALID_SOCKET_VALUE
#define INVALID_SOCKET_VALUE (-1)
#endif
#ifndef SOCKET_ERROR_VALUE
#define SOCKET_ERROR_VALUE (-1)
#endif

inline void ensureWinsock() {}

inline void closeSocketCompat(SocketType socket) {
    shutdown(socket, SHUT_RDWR);
    close(socket);
}

inline ssize_t sendCompat(SocketType socket, const void* data, size_t size) {
    return ::send(socket, data, size, 0);
}

inline ssize_t recvCompat(SocketType socket, void* buffer, size_t size) {
    return ::recv(socket, buffer, size, 0);
}

inline int lastSocketError() {
    return errno;
}

inline bool isWouldBlock(int error) {
    return error == EWOULDBLOCK || error == EAGAIN;
}

inline void setTcpKeepAlive(SocketType socket, int idleSecs, int intervalSecs, int count) {
    int flag = 1;
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
               reinterpret_cast<const char*>(&flag), sizeof(flag));
#ifdef TCP_KEEPIDLE
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE,
               reinterpret_cast<const char*>(&idleSecs), sizeof(int));
#endif
#ifdef TCP_KEEPINTVL
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL,
               reinterpret_cast<const char*>(&intervalSecs), sizeof(int));
#endif
#ifdef TCP_KEEPCNT
    setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT,
               reinterpret_cast<const char*>(&count), sizeof(int));
#endif
}

} // namespace wingman::transport

#endif // Winsock / POSIX
