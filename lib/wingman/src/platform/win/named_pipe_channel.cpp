#include "wingman/ipc/windows/named_pipe_channel.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <exception>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wingman::ipc::windows {

namespace {

bool writeAll(HANDLE handle, const void* data, DWORD size, std::atomic<bool>& stopping) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL result = WriteFile(handle, data, size, &bytesWritten, &overlapped);
    if (!result && GetLastError() == ERROR_IO_PENDING) {
        while (!stopping.load()) {
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
            if (waitResult == WAIT_OBJECT_0) {
                result = GetOverlappedResult(handle, &overlapped, &bytesWritten, FALSE);
                break;
            }
            if (waitResult != WAIT_TIMEOUT) {
                result = FALSE;
                break;
            }
        }
    }

    if (!result || stopping.load()) {
        CancelIoEx(handle, &overlapped);
    }
    CloseHandle(overlapped.hEvent);
    return result && bytesWritten == size && !stopping.load();
}

bool readAll(HANDLE handle, void* data, DWORD size, DWORD& bytesRead, std::atomic<bool>& stopping) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        return false;
    }

    bytesRead = 0;
    BOOL result = ReadFile(handle, data, size, &bytesRead, &overlapped);
    if (!result && GetLastError() == ERROR_IO_PENDING) {
        while (!stopping.load()) {
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
            if (waitResult == WAIT_OBJECT_0) {
                result = GetOverlappedResult(handle, &overlapped, &bytesRead, FALSE);
                break;
            }
            if (waitResult != WAIT_TIMEOUT) {
                result = FALSE;
                break;
            }
        }
    }

    if (!result || stopping.load()) {
        CancelIoEx(handle, &overlapped);
    }
    CloseHandle(overlapped.hEvent);
    return result && bytesRead == size && !stopping.load();
}

} // namespace

NamedPipeChannel::NamedPipeChannel(bool serverMode, const std::string& pipeName)
    : serverMode_(serverMode)
    , pipeName_(pipeName)
    , state_(IpcState::Disconnected)
    , stopping_(false)
    , nextMessageId_(1)
    , pipeHandle_(INVALID_HANDLE_VALUE)
    , ioPending_(false)
{
    // Build Named Pipe path
    fullPipeName_ = "\\\\.\\pipe\\" + pipeName;
}

NamedPipeChannel::~NamedPipeChannel() {
    disconnect();
}

bool NamedPipeChannel::connect(const std::string& endpoint) {
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

void NamedPipeChannel::disconnect() {
    if (state_ == IpcState::Disconnected && pipeHandle_ == INVALID_HANDLE_VALUE) {
        return;
    }

    bool wasConnecting = state_ == IpcState::Connecting;
    setState(IpcState::Disconnecting);
    stopping_ = true;

    stopReceiving();

    if (pipeHandle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(pipeHandle_, nullptr);
        if (serverMode_) {
            DisconnectNamedPipe(pipeHandle_);
        }
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
    }

    setState(IpcState::Disconnected);
    if (!wasConnecting) {
        stopping_ = false;
    }
}

bool NamedPipeChannel::isConnected() const {
    return state_ == IpcState::Connected;
}

IpcState NamedPipeChannel::getState() const {
    return state_;
}

bool NamedPipeChannel::send(const IpcMessage& message) {
    if (!isConnected()) {
        return false;
    }

    // Protect send operations to prevent frame interleaving
    std::lock_guard<std::mutex> lock(sendMutex_);

    std::string json = serializeMessage(message);
    std::vector<uint8_t> data(json.begin(), json.end());

    // Validate message size to prevent memory allocation attacks
    if (data.size() > kMaxMessageSize) {
        spdlog::error("[NamedPipe] Message too large: {} bytes (max: {})", data.size(), kMaxMessageSize);
        setState(IpcState::Error);
        return false;
    }

    // Write message length (4 bytes)
    uint32_t length = static_cast<uint32_t>(data.size());

    if (!writeAll(pipeHandle_, &length, sizeof(length), stopping_)) {
        spdlog::error("[NamedPipe] Failed to write message length: {}", GetLastError());
        setState(IpcState::Error);
        return false;
    }

    // Write message content
    if (!writeAll(pipeHandle_, data.data(), static_cast<DWORD>(data.size()), stopping_)) {
        spdlog::error("[NamedPipe] Failed to write message: {}", GetLastError());
        setState(IpcState::Error);
        return false;
    }

    return true;
}

uint64_t NamedPipeChannel::sendRequest(const std::string& method, const std::string& payload) {
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

bool NamedPipeChannel::sendEvent(const std::string& method, const std::string& payload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Event;
    msg.method = method;
    msg.payload = payload;
    msg.id = 0;
    msg.timestamp = GetTickCount64();

    return send(msg);
}

void NamedPipeChannel::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageCallback_ = std::move(callback);
}

void NamedPipeChannel::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    errorCallback_ = std::move(callback);
}

void NamedPipeChannel::startReceiving() {
    if (receiveThread_.joinable()) {
        return;
    }

    stopping_ = false;
    receiveThread_ = std::thread([this]() { receiveLoop(); });
}

void NamedPipeChannel::stopReceiving() {
    stopping_ = true;

    if (receiveThread_.joinable()) {
        HANDLE nativeHandle = reinterpret_cast<HANDLE>(receiveThread_.native_handle());
        if (pipeHandle_ != INVALID_HANDLE_VALUE && nativeHandle != nullptr) {
            CancelSynchronousIo(nativeHandle);
        }
        eventCondition_.notify_all();
        receiveThread_.join();
    }
}

IpcTransport NamedPipeChannel::getTransport() const {
    return IpcTransport::NamedPipe;
}

std::string NamedPipeChannel::getBackendName() const {
    return "NamedPipe";
}

std::string NamedPipeChannel::getEndpoint() const {
    return pipeName_;
}

void NamedPipeChannel::setState(IpcState state) {
    state_ = state;
    ErrorCallback callback;
    if (state == IpcState::Error) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callback = errorCallback_;
    }
    if (callback) {
        callback("NamedPipe state error");
    }
}

bool NamedPipeChannel::createServerPipe() {
    // Create Named Pipe
    pipeHandle_ = CreateNamedPipeA(
        fullPipeName_.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        65536,  // Output buffer
        65536,  // Input buffer
        0,
        nullptr
    );

    if (pipeHandle_ == INVALID_HANDLE_VALUE) {
        spdlog::error("[NamedPipe] CreateNamedPipe failed: {}", GetLastError());
        setState(IpcState::Error);
        return false;
    }

    serverAccepted_ = false;
    setState(IpcState::Connected);
    spdlog::info("[NamedPipe] Server listening on: {}", fullPipeName_);
    return true;
}

bool NamedPipeChannel::waitForServerClient() {
    if (serverAccepted_) {
        return true;
    }

    spdlog::info("[NamedPipe] Server waiting for connection on: {}", fullPipeName_);

    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        spdlog::error("[NamedPipe] CreateEvent failed: {}", GetLastError());
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
        setState(IpcState::Error);
        return false;
    }

    BOOL connected = ConnectNamedPipe(pipeHandle_, &overlapped);
    if (!connected) {
        DWORD error = GetLastError();
        if (error == ERROR_PIPE_CONNECTED) {
            connected = TRUE;
            SetEvent(overlapped.hEvent);
        } else if (error == ERROR_IO_PENDING) {
            while (!stopping_) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100);
                if (waitResult == WAIT_OBJECT_0) {
                    DWORD bytesTransferred = 0;
                    if (!GetOverlappedResult(pipeHandle_, &overlapped, &bytesTransferred, FALSE)) {
                        error = GetLastError();
                        CloseHandle(overlapped.hEvent);
                        spdlog::error("[NamedPipe] ConnectNamedPipe completion failed: {}", error);
                        CloseHandle(pipeHandle_);
                        pipeHandle_ = INVALID_HANDLE_VALUE;
                        setState(IpcState::Error);
                        return false;
                    }
                    connected = TRUE;
                    break;
                }
                if (waitResult != WAIT_TIMEOUT) {
                    error = GetLastError();
                    CloseHandle(overlapped.hEvent);
                    spdlog::error("[NamedPipe] ConnectNamedPipe wait failed: {}", error);
                    CloseHandle(pipeHandle_);
                    pipeHandle_ = INVALID_HANDLE_VALUE;
                    setState(IpcState::Error);
                    return false;
                }
            }
        } else {
            CloseHandle(overlapped.hEvent);
            spdlog::error("[NamedPipe] ConnectNamedPipe failed: {}", error);
            CloseHandle(pipeHandle_);
            pipeHandle_ = INVALID_HANDLE_VALUE;
            setState(IpcState::Error);
            return false;
        }
    }

    CloseHandle(overlapped.hEvent);
    if (!connected || stopping_) {
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
        setState(IpcState::Disconnected);
        return false;
    }

    serverAccepted_ = true;
    setState(IpcState::Connected);
    spdlog::info("[NamedPipe] Server connected");
    return true;
}

bool NamedPipeChannel::connectToServer() {
    // Wait for Named Pipe to become available
    for (int i = 0; i < 50; ++i) {  // Wait up to 5 seconds
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
            // Wait for pipe to become available
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

    // Set pipe mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipeHandle_, &mode, nullptr, nullptr)) {
        spdlog::warn("[NamedPipe] SetNamedPipeHandleState failed: {}", GetLastError());
    }

    setState(IpcState::Connected);
    spdlog::info("[NamedPipe] Client connected to: {}", fullPipeName_);
    return true;
}

void NamedPipeChannel::receiveLoop() {
    if (serverMode_ && !waitForServerClient()) {
        return;
    }

    while (!stopping_ && isConnected()) {
        // Read message length
        uint32_t messageLength = 0;
        DWORD bytesRead = 0;

        BOOL result = readAll(pipeHandle_, &messageLength, sizeof(messageLength), bytesRead, stopping_);

        if (!result || bytesRead != sizeof(messageLength)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA || error == ERROR_OPERATION_ABORTED) {
                spdlog::info("[NamedPipe] Connection closed");
            } else if (error != ERROR_IO_PENDING) {
                spdlog::error("[NamedPipe] ReadFile length failed: {}", error);
            }
            break;
        }

        // Validate message size to prevent memory allocation attacks
        if (messageLength > kMaxMessageSize) {
            spdlog::error("[NamedPipe] Message too large: {} bytes (max: {})", messageLength, kMaxMessageSize);
            setState(IpcState::Error);
            break;
        }

        // Read message content
        std::vector<uint8_t> buffer(messageLength);
        result = readAll(pipeHandle_, buffer.data(), messageLength, bytesRead, stopping_);

        if (!result || bytesRead != messageLength) {
            spdlog::error("[NamedPipe] ReadFile data failed: {}", GetLastError());
            break;
        }

        // Parse message
        std::string json(buffer.begin(), buffer.end());
        IpcMessage message = deserializeMessage(json);

        MessageCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callback = messageCallback_;
        }
        if (callback) {
            callback(message);
        }
    }

    if (!stopping_) {
        setState(IpcState::Disconnected);
    }
}

std::string NamedPipeChannel::serializeMessage(const IpcMessage& msg) {
    nlohmann::json j;
    j["type"] = static_cast<int>(msg.type);
    j["method"] = msg.method;
    j["id"] = msg.id;
    j["timestamp"] = msg.timestamp;

    if (msg.payload.empty()) {
        j["payload"] = nlohmann::json::object();
    } else {
        try {
            j["payload"] = nlohmann::json::parse(msg.payload);
        } catch (...) {
            j["payload"] = msg.payload;
        }
    }

    return j.dump();
}

IpcMessage NamedPipeChannel::deserializeMessage(const std::string& json) {
    IpcMessage msg;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const std::exception& e) {
        msg.type = IpcMessageType::Error;
        msg.payload = nlohmann::json{{"error", e.what()}}.dump();
        return msg;
    }

    msg.type = static_cast<IpcMessageType>(parsed.value("type", static_cast<int>(IpcMessageType::Request)));
    msg.method = parsed.value("method", "");
    msg.id = parsed.value("id", uint64_t{0});
    msg.timestamp = parsed.value("timestamp", uint64_t{0});

    if (parsed.contains("payload")) {
        msg.payload = parsed["payload"].is_string()
            ? parsed["payload"].get<std::string>()
            : parsed["payload"].dump();
    }
    return msg;
}

} // namespace wingman::ipc::windows

#endif // _WIN32
