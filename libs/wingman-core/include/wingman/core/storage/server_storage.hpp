#pragma once

#include "wingman/storage/storage.hpp"
#include "wingman/server/client.hpp"

#include <memory>
#include <mutex>
#include <functional>

namespace wingman {

// ServerStorage: 服务器全局存储，所有连接的客户端可见
// 适合全局公告、服务器配置、跨团队数据等
class ServerStorage : public IStorage {
public:
    // 使用已连接的 Client
    explicit ServerStorage(std::shared_ptr<server::Client> client);

    ~ServerStorage() override = default;

    // 禁止拷贝
    ServerStorage(const ServerStorage&) = delete;
    ServerStorage& operator=(const ServerStorage&) = delete;

    // IStorage 接口实现
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

    // 额外方法

    // 设置前缀（用于隔离不同类型的数据）
    void setPrefix(const std::string& prefix) { m_prefix = prefix; }
    const std::string& getPrefix() const { return m_prefix; }

    // 订阅键变更事件（全局广播）
    using ChangeCallback = std::function<void(const std::string& key, const std::string& value)>;
    void onChange(const std::string& key, ChangeCallback callback);

    // 全局广播消息（所有连接的客户端都能收到）
    void broadcast(const std::string& event, const std::string& data);

    // 获取客户端
    std::shared_ptr<server::Client> getClient() const { return m_client; }

private:
    std::shared_ptr<server::Client> m_client;
    mutable std::mutex m_mutex;
    std::string m_prefix = "global";

    // 本地缓存（可选，用于减少网络请求）
    mutable std::unordered_map<std::string, std::string> m_cache;
    mutable bool m_cacheEnabled = true;

    // 获取带前缀的完整键
    std::string getFullKey(const std::string& key) const;
};

} // namespace wingman
