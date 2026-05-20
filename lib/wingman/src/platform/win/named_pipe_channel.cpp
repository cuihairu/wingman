#include "wingman/ipc/iipc_channel.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <conditionvariable>

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
    NamedPipeChannel(bool serverMode, const std::string& pipeName)
        : serverMode_(serverMode)
        , pipeName_(pipeName)
        , state_(IpcState::Disconnected)
        , stopping_(false)
        , nextMessageId_(1)
        , pipeHandle_(INVALID_HANDLE_VALUE)
        , ioPending_(false)
    {
        // 构建 Named Pipe 路径
        fullPipeName_ = "\\\\.\\pipe\\" + pipeName;
    }

    ~NamedPipeChannel() override {
        disconnect();
    }

    bool connect(const std::string& endpoint) override {
        if (state_ == IpcState::Connected) {
            return true;
        }

        if (!endpoint.empty()) {
            fullPipeName_ = "\\\\.\\pipe\\" + endpoint;
            pipeName_ = endpoint;
        }

        setState(IpcState::Connecting);

        if (serverMode_) {
            return createServerPipe();
        } else {
            return connectToServer();
        }
    }

    void disconnect() override {
        if (state_ == IpcState::Disconnected) {
            return;
        }

        setState(IpcState::Disconnecting);
        stopping_ = true;

        // 停止接收线程
        if (receiveThread_.joinable()) {
            eventCondition_.notify_all();
            receiveThread_.join();
        }

        // 关闭管道
        if (pipeHandle_ != INVALID_HANDLE_VALUE) {
            if (serverMode_) {
                FlushFileBuffers(pipeHandle_);
                DisconnectNamedPipe(pipeHandle_);
            }
            CloseHandle(pipeHandle_);
            pipeHandle_ = INVALID_HANDLE_VALUE;
        }

        setState(IpcState::Disconnected);
        stopping_ = false;
    }

    bool isConnected() const override {
        return state_ == IpcState::Connected;
    }

    IpcState getState() const override {
        return state_;
    }

    bool send(const IpcMessage& message) override {
        if (!isConnected()) {
            return false;
        }

        std::string json = serializeMessage(message);
        std::vector<uint8_t> data(json.begin(), json.end());

        // 写入消息长度（4 字节）
        uint32_t length = static_cast<uint32_t>(data.size());
        DWORD bytesWritten = 0;

        if (!WriteFile(pipeHandle_, &length, sizeof(length), &bytesWritten, nullptr)) {
            spdlog::error("[NamedPipe] Failed to write message length: {}", GetLastError());
            setState(IpcState::Error);
            return false;
        }

        // 写入消息内容
        if (!WriteFile(pipeHandle_, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr)) {
            spdlog::error("[NamedPipe] Failed to write message: {}", GetLastError());
            setState(IpcState::Error);
            return false;
        }

        return true;
    }

    uint64_t sendRequest(const std::string& method, const std::string& payload) override {
        IpcMessage msg;
        msg.type = IpcMessageType::Request;
        msg.method = method;
        msg.payload = payload;
        msg.id = nextMessageId_++;
        msg.timestamp = GetTickCount64();

        if (send(msg)) {
            return msg.id;
        }
        return 0;
    }

    bool sendEvent(const std::string& method, const std::string& payload) override {
        IpcMessage msg;
        msg.type = IpcMessageType::Event;
        msg.method = method;
        msg.payload = payload;
        msg.id = 0;
        msg.timestamp = GetTickCount64();

        return send(msg);
    }

    void setMessageCallback(MessageCallback callback) override {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        messageCallback_ = std::move(callback);
    }

    void setErrorCallback(ErrorCallback callback) override {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        errorCallback_ = std::move(callback);
    }

    void startReceiving() override {
        if (receiveThread_.joinable()) {
            return;
        }

        stopping_ = false;
        receiveThread_ = std::thread([this]() { receiveLoop(); });
    }

    void stopReceiving() override {
        stopping_ = true;
        eventCondition_.notify_all();

        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
    }

    IpcTransport getTransport() const override {
        return IpcTransport::NamedPipe;
    }

    std::string getBackendName() const override {
        return "NamedPipe";
    }

    std::string getEndpoint() const override {
        return pipeName_;
    }

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

    void setState(IpcState state) {
        state_ = state;
        if (state == IpcState::Error && errorCallback_) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            errorCallback_("NamedPipe state error");
        }
    }

    bool createServerPipe() {
        // 创建 Named Pipe
        pipeHandle_ = CreateNamedPipeA(
            fullPipeName_.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,  // 输出缓冲区
            65536,  // 输入缓冲区
            0,
            nullptr
        );

        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            spdlog::error("[NamedPipe] CreateNamedPipe failed: {}", GetLastError());
            setState(IpcState::Error);
            return false;
        }

        // 等待客户端连接
        spdlog::info("[NamedPipe] Server waiting for connection on: {}", fullPipeName_);

        BOOL connected = ConnectNamedPipe(pipeHandle_, nullptr);
        if (!connected) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            } else {
                spdlog::error("[NamedPipe] ConnectNamedPipe failed: {}", error);
                CloseHandle(pipeHandle_);
                pipeHandle_ = INVALID_HANDLE_VALUE;
                setState(IpcState::Error);
                return false;
            }
        }

        setState(IpcState::Connected);
        spdlog::info("[NamedPipe] Server connected");
        return true;
    }

    bool connectToServer() {
        // 等待 Named Pipe 可用
        for (int i = 0; i < 50; ++i) {  // 最多等待 5 秒
            pipeHandle_ = CreateFileA(
                fullPipeName_.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr
            );

            if (pipeHandle_ != INVALID_HANDLE_VALUE) {
                break;
            }

            DWORD error = GetLastError();
            if (error == ERROR_PIPE_BUSY) {
                // 等待管道可用
                if (!WaitNamedPipeA(fullPipeName_.c_str(), 100)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
            } else {
                spdlog::error("[NamedPipe] CreateFile failed: {}", error);
                setState(IpcState::Error);
                return false;
            }
        }

        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            spdlog::error("[NamedPipe] Failed to connect to server");
            setState(IpcState::Error);
            return false;
        }

        // 设置管道模式
        DWORD mode = PIPE_READMODE_MESSAGE;
        if (!SetNamedPipeHandleState(pipeHandle_, &mode, nullptr, nullptr)) {
            spdlog::warn("[NamedPipe] SetNamedPipeHandleState failed: {}", GetLastError());
        }

        setState(IpcState::Connected);
        spdlog::info("[NamedPipe] Client connected to: {}", fullPipeName_);
        return true;
    }

    void receiveLoop() {
        while (!stopping_ && isConnected()) {
            // 读取消息长度
            uint32_t messageLength = 0;
            DWORD bytesRead = 0;

            BOOL result = ReadFile(
                pipeHandle_,
                &messageLength,
                sizeof(messageLength),
                &bytesRead,
                nullptr
            );

            if (!result || bytesRead != sizeof(messageLength)) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                    spdlog::info("[NamedPipe] Connection closed");
                } else if (error != ERROR_IO_PENDING) {
                    spdlog::error("[NamedPipe] ReadFile length failed: {}", error);
                }
                break;
            }

            // 读取消息内容
            std::vector<uint8_t> buffer(messageLength);
            result = ReadFile(
                pipeHandle_,
                buffer.data(),
                messageLength,
                &bytesRead,
                nullptr
            );

            if (!result || bytesRead != messageLength) {
                spdlog::error("[NamedPipe] ReadFile data failed: {}", GetLastError());
                break;
            }

            // 解析消息
            std::string json(buffer.begin(), buffer.end());
            IpcMessage message = deserializeMessage(json);

            // 调用回调
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (messageCallback_) {
                messageCallback_(message);
            }
        }

        if (!stopping_) {
            setState(IpcState::Disconnected);
        }
    }

    static std::string serializeMessage(const IpcMessage& msg) {
        // 简单 JSON 序列化
        std::string json = "{";
        json += "\"type\":" + std::to_string(static_cast<int>(msg.type)) + ",";
        json += "\"method\":\"" + msg.method + "\",";
        json += "\"payload\":" + (msg.payload.empty() ? "{}" : msg.payload) + ",";
        json += "\"id\":" + std::to_string(msg.id) + ",";
        json += "\"timestamp\":" + std::to_string(msg.timestamp);
        json += "}";
        return json;
    }

    static IpcMessage deserializeMessage(const std::string& json) {
        IpcMessage msg;
        // 简化解析（实际应使用 nlohmann/json）
        // 这里只做演示，实际应该用 JSON 库
        // ...
        return msg;
    }
};

} // namespace wingman::ipc::windows

#endif // _WIN32
