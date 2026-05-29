#pragma once

#include "wingman/storage/storage.hpp"
#include "wingman/server/client.hpp"

#include <memory>
#include <mutex>
#include <functional>

namespace wingman {

// ServerStorage: Server global storage, visible to all connected clients
// Suitable for global announcements, server config, cross-team data, etc.
class ServerStorage : public IStorage {
public:
    // Use connected Client
    explicit ServerStorage(std::shared_ptr<server::Client> client);

    ~ServerStorage() override = default;

    // Non-copyable
    ServerStorage(const ServerStorage&) = delete;
    ServerStorage& operator=(const ServerStorage&) = delete;

    // IStorage interface implementation
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

    // Additional methods

    // Set prefix (for isolating different types of data)
    void setPrefix(const std::string& prefix) { m_prefix = prefix; }
    const std::string& getPrefix() const { return m_prefix; }

    // Subscribe to key change events (global broadcast)
    using ChangeCallback = std::function<void(const std::string& key, const std::string& value)>;
    void onChange(const std::string& key, ChangeCallback callback);

    // Global broadcast message (received by all connected clients)
    void broadcast(const std::string& event, const std::string& data);

    // Get client
    std::shared_ptr<server::Client> getClient() const { return m_client; }

private:
    std::shared_ptr<server::Client> m_client;
    mutable std::mutex m_mutex;
    std::string m_prefix = "global";

    // Local cache (optional, to reduce network requests)
    mutable std::unordered_map<std::string, std::string> m_cache;
    mutable bool m_cacheEnabled = true;

    // Get full key with prefix
    std::string getFullKey(const std::string& key) const;
};

} // namespace wingman
