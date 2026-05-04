#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>

namespace wingman {

// KV 存储选项
struct KvOptions {
    int64_t ttl = 0;        // 过期时间（秒），0 表示不过期
    bool nx = false;        // 仅当 key 不存在时设置 (SET NX)
    bool xx = false;        // 仅当 key 存在时设置 (SET XX)
};

// Hash 操作
using HashFields = std::unordered_map<std::string, std::string>;

// List 操作
using ListValues = std::vector<std::string>;

// 轻量级 KV 存储 (类 Redis)
class KeyValueStore {
public:
    KeyValueStore();
    ~KeyValueStore();

    // ========== 字符串操作 ==========

    // 设置键值
    void set(const std::string& key, const std::string& value, const KvOptions& options = {});

    // 获取值
    std::string get(const std::string& key);

    // 删除键
    void del(const std::string& key);
    void del(const std::vector<std::string>& keys);

    // 检查键是否存在
    bool exists(const std::string& key);

    // 获取匹配的所有键
    std::vector<std::string> keys(const std::string& pattern);

    // 设置过期时间
    void expire(const std::string& key, int64_t seconds);

    // 获取剩余时间
    int64_t ttl(const std::string& key);

    // 自增
    int64_t incr(const std::string& key, int64_t delta = 1);

    // ========== Hash 操作 ==========

    // 设置 hash 字段
    void hset(const std::string& hash, const std::string& field, const std::string& value);

    // 批量设置 hash
    void hmset(const std::string& hash, const HashFields& fields);

    // 获取 hash 字段
    std::string hget(const std::string& hash, const std::string& field);

    // 获取所有 hash 字段
    HashFields hgetall(const std::string& hash);

    // 删除 hash 字段
    void hdel(const std::string& hash, const std::string& field);

    // 检查 hash 字段是否存在
    bool hexists(const std::string& hash, const std::string& field);

    // 获取所有字段名
    std::vector<std::string> hkeys(const std::string& hash);

    // ========== List 操作 ==========

    // 左推入
    void lpush(const std::string& list, const std::string& value);

    // 右推入
    void rpush(const std::string& list, const std::string& value);

    // 左弹出
    std::string lpop(const std::string& list);

    // 右弹出
    std::string rpop(const std::string& list);

    // 获取列表长度
    size_t llen(const std::string& list);

    // 获取列表范围
    ListValues lrange(const std::string& list, int64_t start, int64_t stop);

    // 删除列表元素
    size_t lrem(const std::string& list, int64_t count, const std::string& value);

    // ========== 发布订阅 (简化版) ==========

    using ChannelCallback = std::function<void(const std::string& message)>;

    // 发布消息
    void publish(const std::string& channel, const std::string& message);

    // 订阅频道
    void subscribe(const std::string& channel, ChannelCallback callback);

    // 取消订阅
    void unsubscribe(const std::string& channel);

    // ========== 持久化 ==========

    // 保存到 SQLite
    bool save(const std::string& filepath);

    // 从 SQLite 加载
    bool load(const std::string& filepath);

    // 开启自动持久化
    void enableAutoSave(const std::string& filepath, int64_t intervalSeconds);

    // ========== 维护 ==========

    // 清理过期键
    size_t cleanupExpired();

    // 获取统计信息
    std::unordered_map<std::string, size_t> stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace wingman
