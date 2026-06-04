#pragma once

#include "wingman/ipc/iipc_channel.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace wingman::ipc {

class TcpChannel : public IIpcChannel {
public:
    TcpChannel(bool serverMode, const std::string& host, int port);
    ~TcpChannel() override;

    bool connect(const std::string& endpoint) override;
    void disconnect() override;

    bool isConnected() const override;
    IpcState getState() const override;

    bool send(const IpcMessage& message) override;
    uint64_t sendRequest(const std::string& method, const std::string& payload = "{}") override;
    bool sendEvent(const std::string& method, const std::string& payload = "{}") override;

    void setMessageCallback(MessageCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

    void startReceiving() override;
    void stopReceiving() override;

    IpcTransport getTransport() const override;
    std::string getBackendName() const override;
    std::string getEndpoint() const override;

private:
    bool serverMode_;
    std::string host_;
    int port_;
    std::string endpoint_;
    std::atomic<IpcState> state_;
    std::atomic<bool> stopping_;
    std::atomic<uint64_t> nextMessageId_;

    socket_t listenSocket_ = INVALID_SOCKET;
    socket_t dataSocket_ = INVALID_SOCKET;

    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    std::mutex callbacksMutex_;
    std::mutex sendMutex_;

    std::thread receiveThread_;

    void setState(IpcState state);
    bool createServer();
    bool connectToServer();
    void receiveLoop();

    bool sendRaw(const void* data, size_t len);
    bool recvRaw(void* data, size_t len);

    static std::string serializeMessage(const IpcMessage& msg);
    static IpcMessage deserializeMessage(const std::string& json);
};

} // namespace wingman::ipc
