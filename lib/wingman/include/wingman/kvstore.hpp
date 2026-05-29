#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <memory>

namespace wingman {

// KV storage options
struct KvOptions {
    int64_t ttl = 0;        // Expiration time (seconds), 0 means no expiration
    bool nx = false;        // Set only if key does not exist (SET NX)
    bool xx = false;        // Set only if key exists (SET XX)
};

// Hash operations
using HashFields = std::unordered_map<std::string, std::string>;

// List operations
using ListValues = std::vector<std::string>;

// Lightweight KV store (Redis-like)
class KeyValueStore {
public:
    KeyValueStore();
    ~KeyValueStore();

    // ========== String operations ==========

    // Set key-value
    void set(const std::string& key, const std::string& value, const KvOptions& options = {});

    // Get value
    std::string get(const std::string& key);

    // Delete key
    void del(const std::string& key);
    void del(const std::vector<std::string>& keys);

    // Check if key exists
    bool exists(const std::string& key);

    // Get all matching keys
    std::vector<std::string> keys(const std::string& pattern);

    // Set expiration time
    void expire(const std::string& key, int64_t seconds);

    // Get remaining time
    int64_t ttl(const std::string& key);

    // Increment
    int64_t incr(const std::string& key, int64_t delta = 1);

    // ========== Hash operations ==========

    // Set hash field
    void hset(const std::string& hash, const std::string& field, const std::string& value);

    // Batch set hash
    void hmset(const std::string& hash, const HashFields& fields);

    // Get hash field
    std::string hget(const std::string& hash, const std::string& field);

    // Get all hash fields
    HashFields hgetall(const std::string& hash);

    // Delete hash field
    void hdel(const std::string& hash, const std::string& field);

    // Check if hash field exists
    bool hexists(const std::string& hash, const std::string& field);

    // Get all field names
    std::vector<std::string> hkeys(const std::string& hash);

    // ========== List operations ==========

    // Left push
    void lpush(const std::string& list, const std::string& value);

    // Right push
    void rpush(const std::string& list, const std::string& value);

    // Left pop
    std::string lpop(const std::string& list);

    // Right pop
    std::string rpop(const std::string& list);

    // Get list length
    size_t llen(const std::string& list);

    // Get list range
    ListValues lrange(const std::string& list, int64_t start, int64_t stop);

    // Delete list element
    size_t lrem(const std::string& list, int64_t count, const std::string& value);

    // ========== Pub/Sub (simplified) ==========

    using ChannelCallback = std::function<void(const std::string& message)>;

    // Publish message
    void publish(const std::string& channel, const std::string& message);

    // Subscribe channel
    void subscribe(const std::string& channel, ChannelCallback callback);

    // Unsubscribe
    void unsubscribe(const std::string& channel);

    // ========== Persistence ==========

    // Save to SQLite
    bool save(const std::string& filepath);

    // Load from SQLite
    bool load(const std::string& filepath);

    // Enable auto-persistence
    void enableAutoSave(const std::string& filepath, int64_t intervalSeconds);

    // ========== Maintenance ==========

    // Clean up expired keys
    size_t cleanupExpired();

    // Get statistics
    std::unordered_map<std::string, size_t> stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace wingman
