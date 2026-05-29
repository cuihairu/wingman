#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace wingman::ipc {

/**
 * @brief IPC transport type
 */
enum class IpcTransport : uint8_t {
    Auto,           // Auto-select best option
    NamedPipe,      // Windows Named Pipe
    UnixSocket,     // Unix Domain Socket
    TcpPipe         // TCP (fallback)
};

/**
 * @brief IPC message type
 */
enum class IpcMessageType : uint8_t {
    Request,        // Request
    Response,       // Response
    Event,          // Event (one-way)
    Error           // Error
};

/**
 * @brief IPC message
 */
struct IpcMessage {
    IpcMessageType type;
    std::string method;       // Method name (e.g. "script.start"）
    std::string payload;      // JSON payload
    uint64_t id;             // Message ID (request/response matching)
    uint64_t timestamp;      // Timestamp

    IpcMessage() : type(IpcMessageType::Request), id(0), timestamp(0) {}
};

/**
 * @brief Message callback
 */
using MessageCallback = std::function<void(const IpcMessage&)>;

/**
 * @brief Error callback
 */
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief IPC state
 */
enum class IpcState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief IPC channel interface
 *
 * Abstract inter-process communication channel, supports multiple transport methods.
 */
class IIpcChannel {
public:
    virtual ~IIpcChannel() = default;

    // ========== Connection management ==========

    /**
     * @brief Connect to server
     * @param endpoint Endpoint (Named Pipe name / Unix Socket path / TCP address)
     * @return Returns true on success
     */
    virtual bool connect(const std::string& endpoint) = 0;

    /**
     * @brief Disconnect
     */
    virtual void disconnect() = 0;

    /**
     * @brief Check connection state
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get current state
     */
    virtual IpcState getState() const = 0;

    // ========== Message sending ==========

    /**
     * @brief Send message
     * @param message Message content
     * @return Returns true on success
     */
    virtual bool send(const IpcMessage& message) = 0;

    /**
     * @brief Send request (auto-generates ID)
     * @param method Method name
     * @param payload JSON payload
     * @return Message ID, returns 0 on failure
     */
    virtual uint64_t sendRequest(const std::string& method, const std::string& payload = "{}") = 0;

    /**
     * @brief Send event (one-way)
     * @param method Event name
     * @param payload JSON payload
     * @return Returns true on success
     */
    virtual bool sendEvent(const std::string& method, const std::string& payload = "{}") = 0;

    // ========== Message receiving ==========

    /**
     * @brief Set message callback
     * @param callback Callback function
     */
    virtual void setMessageCallback(MessageCallback callback) = 0;

    /**
     * @brief Set error callback
     * @param callback Callback function
     */
    virtual void setErrorCallback(ErrorCallback callback) = 0;

    /**
     * @brief Start receiving messages (async)
     */
    virtual void startReceiving() = 0;

    /**
     * @brief Stop receiving messages
     */
    virtual void stopReceiving() = 0;

    // ========== Backend information ==========

    /**
     * @brief Get transport type
     */
    virtual IpcTransport getTransport() const = 0;

    /**
     * @brief Get backend name
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief Get endpoint
     */
    virtual std::string getEndpoint() const = 0;
};

} // namespace wingman::ipc
