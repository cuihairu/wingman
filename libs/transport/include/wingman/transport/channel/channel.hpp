#pragma once

// 避免 Windows 宏冲突
#ifdef _WIN32
#ifdef Get
#undef Get
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include "wingman/transport/session/session.hpp"
#include <map>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <future>

namespace wingman::transport {

// ========== Channel (消息通道) ==========
// 用于多路复用和请求-响应匹配

// 类型别名
using ChannelId = uint64_t;

// 前向声明
class Channel;

using ChannelPtr = std::shared_ptr<Channel>;

enum class ChannelType : uint8_t {
    RequestResponse,    // 请求-响应模式
    Stream,             // 流式模式
    PubSub              // 发布-订阅模式
};

class Channel {
    friend class ChannelManager;

public:
    Channel(ChannelId id, ChannelType type, std::weak_ptr<Session> session)
        : id_(id), type_(type), session_(session) {}

    ~Channel() {
        close();
    }

    ChannelId getId() const { return id_; }
    ChannelType getType() const { return type_; }

    // 发送请求并等待响应
    std::future<MessagePtr> request(const MessagePtr& request) {
        auto promise = std::make_shared<std::promise<MessagePtr>>();

        // 设置序列号
        request->header.sequence = nextSequence_++;

        // 保存 promise
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingRequests_[request->header.sequence] = promise;
        }

        // 发送
        if (auto session = session_.lock()) {
            session->send(request);
        } else {
            promise->set_value(nullptr);
        }

        return promise->get_future();
    }

    // 发送通知（不需要响应）
    void notify(const MessagePtr& message) {
        message->header.type = MessageType::Notify;

        if (auto session = session_.lock()) {
            session->send(message);
        }
    }

    // 处理响应（由 Session 调用）
    void handleResponse(const MessagePtr& response) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pendingRequests_.find(response->header.sequence);
        if (it != pendingRequests_.end()) {
            it->second->set_value(response);
            pendingRequests_.erase(it);
        }
    }

    // 关闭通道
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 取消所有等待的请求
        for (auto& [seq, promise] : pendingRequests_) {
            promise->set_value(nullptr);
        }
        pendingRequests_.clear();
    }

private:
    ChannelId id_;
    ChannelType type_;
    std::weak_ptr<Session> session_;
    std::mutex mutex_;
    uint32_t nextSequence_ = 1;
    std::map<uint32_t, std::shared_ptr<std::promise<MessagePtr>>> pendingRequests_;
};

// ========== Channel Manager ==========

class ChannelManager {
public:
    ChannelManager() = default;
    ~ChannelManager() {
        closeAll();
    }

    // 创建通道
    ChannelPtr createChannel(ChannelId id, ChannelType type, std::weak_ptr<Session> session) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto channel = std::make_shared<Channel>(id, type, session);
        channels_[id] = channel;
        return channel;
    }

    // 获取通道
    ChannelPtr getChannel(ChannelId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(id);
        return it != channels_.end() ? it->second : nullptr;
    }

    // 关闭通道
    void closeChannel(ChannelId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(id);
        if (it != channels_.end()) {
            it->second->close();
            channels_.erase(it);
        }
    }

    // 关闭所有通道
    void closeAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, channel] : channels_) {
            channel->close();
        }
        channels_.clear();
    }

    // 移除会话相关的所有通道
    void removeBySession(SessionId sessionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = channels_.begin(); it != channels_.end();) {
            if (auto session = it->second->session_.lock(); !session) {
                it = channels_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::mutex mutex_;
    std::map<ChannelId, ChannelPtr> channels_;
};

} // namespace wingman::transport
