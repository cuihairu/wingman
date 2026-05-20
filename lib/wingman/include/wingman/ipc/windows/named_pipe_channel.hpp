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
#include <windows.h>

namespace wingman::ipc::windows {

/**
 * @brief Windows Named Pipe 实现
 *
 * 使用 Named Pipe 进行本地进程间通信。
 * 支持异步 IO 和消息传递。
 */
class NamedPipeChannel : public IIpcChannel {
public:
    NamedPipeChannel(bool serverMode, const std::string& pipeName);
    ~NamedPipeChannel() override;

    bool connect(const std::string& endpoint) override;
    void disconnect() override;

    bool isConnected() const override;
    IpcState getState() const override;

    bool send(const IpcMessage& message) override;
    uint64_t sendRequest(const std::string& method, const std::string& payload) override;
    bool sendEvent(const std::string& method, const std::string& payload) override;

    void setMessageCallback(MessageCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

    void startReceiving() override;
    void stopReceiving() override;

    IpcTransport getTransport() const override;
    std::string getBackendName() const override;
    std::string getEndpoint() const override;

private:
    bool serverMode_;
    std::string pipeName_;
    std::string fullPipeName_;
    std::atomic<IpcState> state_;
    std::atomic<bool> stopping_;
    std::atomic<uint64_t> nextMessageId_;

    HANDLE pipeHandle_;
    OVERLAPPED overlapped_;
    std::atomic<bool> ioPending_;

    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
    std::mutex callbacksMutex_;

    std::thread receiveThread_;
    std::queue<std::vector<uint8_t>> receiveQueue_;
    std::mutex receiveMutex_;
    std::condition_variable eventCondition_;

    void setState(IpcState state);
    bool createServerPipe();
    bool connectToServer();
    void receiveLoop();

    static std::string serializeMessage(const IpcMessage& msg);
    static IpcMessage deserializeMessage(const std::string& json);
};

} // namespace wingman::ipc::windows

#endif // _WIN32
