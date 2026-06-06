#pragma once

#include "wingman/ipc/iipc_channel.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace wingman::ipc {

/**
 * @brief Unix Domain Socket IPC channel
 *
 * Provides IPC via Unix Domain Sockets for macOS/Linux.
 */
class UnixSocketChannel : public IIpcChannel {
public:
    UnixSocketChannel(bool serverMode, const std::string& socketPath);
    ~UnixSocketChannel() override;

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
    std::string socketPath_;
    std::atomic<IpcState> state_;
    std::atomic<bool> stopping_;
    std::atomic<uint64_t> nextMessageId_;

    int listenFd_ = -1;
    int dataFd_ = -1;

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

#endif // __unix__ || __APPLE__
