#pragma once

#include "wingman/storage/storage.hpp"

#include <unordered_map>
#include <mutex>

namespace wingman {

// SessionStorage: 内存存储，进程退出后数据清空
// 类似浏览器的 sessionStorage，适合临时会话数据
class SessionStorage : public IStorage {
public:
    SessionStorage() = default;
    ~SessionStorage() override = default;

    // 禁止拷贝
    SessionStorage(const SessionStorage&) = delete;
    SessionStorage& operator=(const SessionStorage&) = delete;

    // IStorage 接口实现
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_data;
};

} // namespace wingman
