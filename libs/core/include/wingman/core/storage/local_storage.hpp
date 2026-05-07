#pragma once

#include "wingman/storage/storage.hpp"

#include <unordered_map>
#include <mutex>
#include <string>
#include <filesystem>

namespace wingman {

// LocalStorage: 文件持久化存储，进程重启后数据保留
// 类似浏览器的 localStorage，适合本地配置和缓存
class LocalStorage : public IStorage {
public:
    // 构造函数，指定存储目录
    explicit LocalStorage(const std::filesystem::path& storageDir = "storage");
    ~LocalStorage() override;

    // 禁止拷贝
    LocalStorage(const LocalStorage&) = delete;
    LocalStorage& operator=(const LocalStorage&) = delete;

    // IStorage 接口实现
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

    // 额外方法

    // 立即保存到磁盘
    bool save();

    // 从磁盘重新加载
    bool load();

    // 获取存储路径
    const std::filesystem::path& getStoragePath() const { return m_storagePath; }

    // 设置命名空间（用于隔离不同模块的数据）
    void setNamespace(const std::string& ns) { m_namespace = ns; }
    const std::string& getNamespace() const { return m_namespace; }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_data;
    std::filesystem::path m_storagePath;
    std::string m_namespace;

    // 获取带命名空间的完整键
    std::string getFullKey(const std::string& key) const;

    // 获取存储文件路径
    std::filesystem::path getStorageFile() const;
};

} // namespace wingman
