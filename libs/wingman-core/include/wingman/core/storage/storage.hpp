#pragma once

#include <string>
#include <vector>
#include <optional>

namespace wingman {

// 存储接口 (类似浏览器 Web Storage API)
class IStorage {
public:
    virtual ~IStorage() = default;

    // 获取项数
    virtual size_t length() const = 0;

    // 获取所有键
    virtual std::vector<std::string> keys() const = 0;

    // 获取值 (不存在返回 nullopt)
    virtual std::optional<std::string> getItem(const std::string& key) const = 0;

    // 设置值 (返回是否成功)
    virtual bool setItem(const std::string& key, const std::string& value) = 0;

    // 删除值 (返回是否存在)
    virtual bool removeItem(const std::string& key) = 0;

    // 清空所有数据
    virtual void clear() = 0;

    // 检查键是否存在
    virtual bool hasItem(const std::string& key) const = 0;
};

} // namespace wingman
