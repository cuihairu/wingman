#include "wingman/kvstore.hpp"

#include <unordered_map>
#include <list>
#include <mutex>
#include <regex>
#include <thread>
#include <condition_variable>
#ifdef WINGMAN_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace wingman {

// KV item (with expiration)
struct KvItem {
    std::string value;
    int64_t expireTime;  // 0 means no expiration, otherwise expiration timestamp

    KvItem() : expireTime(0) {}
    KvItem(const std::string& v, int64_t exp) : value(v), expireTime(exp) {}

    bool isExpired() const {
        return expireTime > 0 && expireTime < std::time(nullptr);
    }
};

// Hash item
struct KvHashItem {
    std::unordered_map<std::string, std::string> fields;
};

// List item
struct KvListItem {
    std::list<std::string> values;
};

// Subscription info
struct Subscription {
    std::string channel;
    KeyValueStore::ChannelCallback callback;
};

// KeyValueStore implementation
class KeyValueStore::Impl {
public:
    std::unordered_map<std::string, KvItem> store;
    std::unordered_map<std::string, KvHashItem> hashes;
    std::unordered_map<std::string, KvListItem> lists;
    std::vector<Subscription> subscriptions;

    std::mutex mutex;
    std::thread cleanupThread;
    std::thread saveThread;
    std::atomic<bool> running{false};
    std::string autoSavePath;
    int64_t autoSaveInterval;

    Impl() : autoSaveInterval(0) {}

    void cleanupExpired() {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = store.begin();
        while (it != store.end()) {
            if (it->second.isExpired()) {
                it = store.erase(it);
            } else {
                ++it;
            }
        }
    }

#ifdef WINGMAN_HAS_SQLITE
    struct SqliteRaii {
        sqlite3* db = nullptr;
        explicit SqliteRaii(sqlite3* d) : db(d) {}
        ~SqliteRaii() { if (db) sqlite3_close(db); }
        SqliteRaii(const SqliteRaii&) = delete;
        SqliteRaii& operator=(const SqliteRaii&) = delete;
    };

    bool saveToSqlite(const std::string& filepath) {
        sqlite3* raw = nullptr;
        if (sqlite3_open(filepath.c_str(), &raw) != SQLITE_OK) {
            return false;
        }
        SqliteRaii db(raw);

        const char* createTable =
            "CREATE TABLE IF NOT EXISTS kv_store ("
            "key TEXT PRIMARY KEY, value TEXT, expire_time INTEGER);";
        if (sqlite3_exec(db.db, createTable, nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_exec(db.db, "BEGIN;", nullptr, nullptr, nullptr);
        sqlite3_exec(db.db, "DELETE FROM kv_store;", nullptr, nullptr, nullptr);

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db.db, "INSERT INTO kv_store (key, value, expire_time) VALUES (?, ?, ?);",
                               -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto& [key, item] : store) {
                sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, item.value.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, item.expireTime);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_exec(db.db, "COMMIT;", nullptr, nullptr, nullptr);
        return true;
    }

    bool loadFromSqlite(const std::string& filepath) {
        sqlite3* raw = nullptr;
        if (sqlite3_open(filepath.c_str(), &raw) != SQLITE_OK) {
            return false;
        }
        SqliteRaii db(raw);

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db.db, "SELECT key, value, expire_time FROM kv_store;",
                               -1, &stmt, nullptr) == SQLITE_OK) {
            std::lock_guard<std::mutex> lock(mutex);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int64_t expireTime = sqlite3_column_int64(stmt, 2);

                if (key && value) {
                    store[key] = KvItem(value, expireTime);
                }
            }
            sqlite3_finalize(stmt);
        }

        return true;
    }
#endif // WINGMAN_HAS_SQLITE
};

KeyValueStore::KeyValueStore() : m_impl(std::make_unique<Impl>()) {}

KeyValueStore::~KeyValueStore() {
    // Signal threads to stop
    m_impl->running.store(false);

    // Wait for cleanup thread to finish
    if (m_impl->cleanupThread.joinable()) {
        m_impl->cleanupThread.join();
    }

    // Wait for save thread to finish
    if (m_impl->saveThread.joinable()) {
        m_impl->saveThread.join();
    }
}

// String operations
void KeyValueStore::set(const std::string& key, const std::string& value, const KvOptions& options) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    int64_t expireTime = 0;
    if (options.ttl > 0) {
        expireTime = std::time(nullptr) + options.ttl;
    }

    if (options.nx) {
        // Set only if key does not exist
        if (m_impl->store.find(key) == m_impl->store.end()) {
            m_impl->store[key] = KvItem(value, expireTime);
        }
    } else if (options.xx) {
        // Set only if key exists
        if (m_impl->store.find(key) != m_impl->store.end()) {
            m_impl->store[key] = KvItem(value, expireTime);
        }
    } else {
        m_impl->store[key] = KvItem(value, expireTime);
    }
}

std::string KeyValueStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->store.find(key);
    if (it == m_impl->store.end()) {
        return "";
    }

    if (it->second.isExpired()) {
        m_impl->store.erase(it);
        return "";
    }

    return it->second.value;
}

void KeyValueStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->store.erase(key);
    m_impl->hashes.erase(key);
    m_impl->lists.erase(key);
}

void KeyValueStore::del(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    for (const auto& key : keys) {
        m_impl->store.erase(key);
        m_impl->hashes.erase(key);
        m_impl->lists.erase(key);
    }
}

bool KeyValueStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->store.find(key);
    if (it == m_impl->store.end()) {
        return false;
    }

    if (it->second.isExpired()) {
        m_impl->store.erase(it);
        return false;
    }

    return true;
}

std::vector<std::string> KeyValueStore::keys(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    std::vector<std::string> result;
    std::regex regex(pattern);

    for (const auto& [key, item] : m_impl->store) {
        if (!item.isExpired() && std::regex_match(key, regex)) {
            result.push_back(key);
        }
    }

    return result;
}

void KeyValueStore::expire(const std::string& key, int64_t seconds) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->store.find(key);
    if (it != m_impl->store.end()) {
        it->second.expireTime = std::time(nullptr) + seconds;
    }
}

int64_t KeyValueStore::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->store.find(key);
    if (it == m_impl->store.end() || it->second.expireTime == 0) {
        return -1;  // Does not exist or no expiration set
    }

    int64_t remaining = it->second.expireTime - std::time(nullptr);
    return remaining > 0 ? remaining : -2;  // -2 means expired
}

int64_t KeyValueStore::incr(const std::string& key, int64_t delta) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    int64_t value = 0;
    auto it = m_impl->store.find(key);
    if (it != m_impl->store.end() && !it->second.isExpired()) {
        try {
            value = std::stoll(it->second.value);
        } catch (...) {}
    }

    value += delta;
    m_impl->store[key] = KvItem(std::to_string(value), 0);
    return value;
}

// Hash operations
void KeyValueStore::hset(const std::string& hash, const std::string& field, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->hashes[hash].fields[field] = value;
}

void KeyValueStore::hmset(const std::string& hash, const HashFields& fields) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    for (const auto& [field, value] : fields) {
        m_impl->hashes[hash].fields[field] = value;
    }
}

std::string KeyValueStore::hget(const std::string& hash, const std::string& field) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->hashes.find(hash);
    if (it == m_impl->hashes.end()) {
        return "";
    }

    auto fit = it->second.fields.find(field);
    if (fit == it->second.fields.end()) {
        return "";
    }

    return fit->second;
}

HashFields KeyValueStore::hgetall(const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->hashes.find(hash);
    if (it == m_impl->hashes.end()) {
        return {};
    }

    return it->second.fields;
}

void KeyValueStore::hdel(const std::string& hash, const std::string& field) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->hashes.find(hash);
    if (it != m_impl->hashes.end()) {
        it->second.fields.erase(field);
        if (it->second.fields.empty()) {
            m_impl->hashes.erase(it);
        }
    }
}

bool KeyValueStore::hexists(const std::string& hash, const std::string& field) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->hashes.find(hash);
    if (it == m_impl->hashes.end()) {
        return false;
    }

    return it->second.fields.find(field) != it->second.fields.end();
}

std::vector<std::string> KeyValueStore::hkeys(const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->hashes.find(hash);
    if (it == m_impl->hashes.end()) {
        return {};
    }

    std::vector<std::string> result;
    for (const auto& [field, _] : it->second.fields) {
        result.push_back(field);
    }
    return result;
}

// List operations
void KeyValueStore::lpush(const std::string& list, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->lists[list].values.push_front(value);
}

void KeyValueStore::rpush(const std::string& list, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->lists[list].values.push_back(value);
}

std::string KeyValueStore::lpop(const std::string& list) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->lists.find(list);
    if (it == m_impl->lists.end() || it->second.values.empty()) {
        return "";
    }

    std::string value = it->second.values.front();
    it->second.values.pop_front();

    if (it->second.values.empty()) {
        m_impl->lists.erase(it);
    }

    return value;
}

std::string KeyValueStore::rpop(const std::string& list) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->lists.find(list);
    if (it == m_impl->lists.end() || it->second.values.empty()) {
        return "";
    }

    std::string value = it->second.values.back();
    it->second.values.pop_back();

    if (it->second.values.empty()) {
        m_impl->lists.erase(it);
    }

    return value;
}

size_t KeyValueStore::llen(const std::string& list) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->lists.find(list);
    if (it == m_impl->lists.end()) {
        return 0;
    }

    return it->second.values.size();
}

ListValues KeyValueStore::lrange(const std::string& list, int64_t start, int64_t stop) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    ListValues result;
    auto it = m_impl->lists.find(list);
    if (it == m_impl->lists.end()) {
        return result;
    }

    auto& values = it->second.values;
    size_t size = values.size();

    // Handle negative index
    if (start < 0) start = size + start;
    if (stop < 0) stop = size + stop;
    if (stop < 0) stop = -1;  // To end

    size_t startIndex = static_cast<size_t>(std::max<int64_t>(0, start));
    size_t stopIndex = static_cast<size_t>(std::max<int64_t>(0, stop));

    for (size_t i = startIndex; i <= stopIndex && i < size; ++i) {
        auto vit = values.begin();
        std::advance(vit, i);
        result.push_back(*vit);
    }

    return result;
}

size_t KeyValueStore::lrem(const std::string& list, int64_t count, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->lists.find(list);
    if (it == m_impl->lists.end()) {
        return 0;
    }

    size_t removed = 0;
    auto& values = it->second.values;

    if (count == 0) {
        // Remove all elements equal to value
        auto vit = values.begin();
        while (vit != values.end()) {
            if (*vit == value) {
                vit = values.erase(vit);
                removed++;
            } else {
                ++vit;
            }
        }
    } else if (count > 0) {
        // Remove count elements from head
        int64_t toRemove = count;
        auto vit = values.begin();
        while (vit != values.end() && toRemove > 0) {
            if (*vit == value) {
                vit = values.erase(vit);
                removed++;
                toRemove--;
            } else {
                ++vit;
            }
        }
    } else {
        // count < 0, remove |count| elements from tail
        int64_t toRemove = -count;
        auto vit = values.rbegin();
        while (vit != values.rend() && toRemove > 0) {
            if (*vit == value) {
                // Reverse iterator cannot erase directly
                auto forwardIt = std::next(vit).base();
                values.erase(std::prev(forwardIt));
                removed++;
                toRemove--;
            } else {
                ++vit;
            }
        }
    }

    if (values.empty()) {
        m_impl->lists.erase(it);
    }

    return removed;
}

// Pub/Sub
void KeyValueStore::publish(const std::string& channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (auto& sub : m_impl->subscriptions) {
        if (sub.channel == channel) {
            sub.callback(message);
        }
    }
}

void KeyValueStore::subscribe(const std::string& channel, ChannelCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    Subscription sub;
    sub.channel = channel;
    sub.callback = callback;
    m_impl->subscriptions.push_back(sub);
}

void KeyValueStore::unsubscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    m_impl->subscriptions.erase(
        std::remove_if(m_impl->subscriptions.begin(), m_impl->subscriptions.end(),
            [&channel](const Subscription& sub) { return sub.channel == channel; }),
        m_impl->subscriptions.end()
    );
}

// Persistence
bool KeyValueStore::save(const std::string& filepath) {
#ifdef WINGMAN_HAS_SQLITE
    return m_impl->saveToSqlite(filepath);
#else
    (void)filepath;
    return false;
#endif
}

bool KeyValueStore::load(const std::string& filepath) {
#ifdef WINGMAN_HAS_SQLITE
    return m_impl->loadFromSqlite(filepath);
#else
    (void)filepath;
    return false;
#endif
}

void KeyValueStore::enableAutoSave(const std::string& filepath, int64_t intervalSeconds) {
    // Prevent restarting if already running to avoid thread overwrite
    if (m_impl->running.load()) {
        // Already running - update configuration without restarting threads
        m_impl->autoSavePath = filepath;
        m_impl->autoSaveInterval = intervalSeconds;
        return;
    }

    // Validate interval to prevent busy loop
    if (intervalSeconds <= 0) {
        intervalSeconds = 60; // Default to 60 seconds if invalid
    }

    m_impl->autoSavePath = filepath;
    m_impl->autoSaveInterval = intervalSeconds;
    m_impl->running.store(true);

    // Start cleanup thread
    m_impl->cleanupThread = std::thread([this]() {
        while (m_impl->running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (m_impl->running.load()) {
                m_impl->cleanupExpired();
            }
        }
    });

    // Start auto-save thread
    m_impl->saveThread = std::thread([this]() {
        while (m_impl->running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(m_impl->autoSaveInterval));
            if (m_impl->running.load() && !m_impl->autoSavePath.empty()) {
                save(m_impl->autoSavePath);
            }
        }
    });
}

size_t KeyValueStore::cleanupExpired() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    size_t count = 0;

    auto it = m_impl->store.begin();
    while (it != m_impl->store.end()) {
        if (it->second.isExpired()) {
            it = m_impl->store.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    return count;
}

std::unordered_map<std::string, size_t> KeyValueStore::stats() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    std::unordered_map<std::string, size_t> result;
    result["strings"] = m_impl->store.size();
    result["hashes"] = m_impl->hashes.size();
    result["lists"] = m_impl->lists.size();
    result["subscriptions"] = m_impl->subscriptions.size();

    return result;
}

} // namespace wingman
