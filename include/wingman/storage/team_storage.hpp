#pragma once

#include "wingman/storage/storage.hpp"
#include "wingman/server/client.hpp"

#include <memory>
#include <mutex>
#include <functional>
#include <optional>

namespace wingman {

// TeamStorage: 远程同步存储，全队共享
// 通过 server/orchestrator 同步数据，所有队员可见
// 适合协同任务、投票状态、共享配置等
class TeamStorage : public IStorage {
public:
    // 使用已连接的 Client
    explicit TeamStorage(std::shared_ptr<server::Client> client);

    ~TeamStorage() override = default;

    // 禁止拷贝
    TeamStorage(const TeamStorage&) = delete;
    TeamStorage& operator=(const TeamStorage&) = delete;

    // IStorage 接口实现
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

    // 额外方法

    // 设置前缀（用于隔离不同队伍/频道的数据）
    void setPrefix(const std::string& prefix) { m_prefix = prefix; }
    const std::string& getPrefix() const { return m_prefix; }

    // 订阅键变更事件
    using ChangeCallback = std::function<void(const std::string& key, const std::string& value)>;
    void onChange(const std::string& key, ChangeCallback callback);

    // 广播消息到全队
    void broadcast(const std::string& event, const std::string& data);

    // 获取客户端
    std::shared_ptr<server::Client> getClient() const { return m_client; }

private:
    std::shared_ptr<server::Client> m_client;
    mutable std::mutex m_mutex;
    std::string m_prefix = "team";

    // 本地缓存（可选，用于减少网络请求）
    mutable std::unordered_map<std::string, std::string> m_cache;
    mutable bool m_cacheEnabled = true;

    // 获取带前缀的完整键
    std::string getFullKey(const std::string& key) const;
};

} // namespace wingman
