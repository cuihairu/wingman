// Remote storage implementation - requires server module
// Only compiled when WINGMAN_BUILD_SERVER is enabled

#include "wingman/storage/team_storage.hpp"
#include "wingman/storage/server_storage.hpp"
#include "wingman/storage/storage_all.hpp"

namespace wingman {

// ========== TeamStorage Implementation ==========

TeamStorage::TeamStorage(std::shared_ptr<server::Client> client)
    : m_client(client) {
    if (!m_client) {
        throw std::invalid_argument("Client cannot be null");
    }
}

std::string TeamStorage::getFullKey(const std::string& key) const {
    if (m_prefix.empty()) {
        return "team:" + key;
    }
    return "team:" + m_prefix + ":" + key;
}

size_t TeamStorage::length() const {
    // Query team key count via client
    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "keys"}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK &&
            response.data.contains("result") && response.data["result"].is_array()) {
            return response.data["result"].size();
        }
    } catch (...) {}

    return 0;
}

std::vector<std::string> TeamStorage::keys() const {
    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "keys"}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK &&
            response.data.contains("result") && response.data["result"].is_array()) {
            return response.data["result"].get<std::vector<std::string>>();
        }
    } catch (...) {}

    return {};
}

std::optional<std::string> TeamStorage::getItem(const std::string& key) const {
    std::string fullKey = getFullKey(key);

    // Check cache first
    if (m_cacheEnabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(fullKey);
        if (it != m_cache.end()) {
            return it->second;
        }
    }

    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "get"}, {"key", fullKey}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK && response.data.contains("result")) {
            std::string value = response.data["result"].get<std::string>();

            // Update cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[fullKey] = value;
            }

            return value;
        }
    } catch (...) {}

    return std::nullopt;
}

bool TeamStorage::setItem(const std::string& key, const std::string& value) {
    std::string fullKey = getFullKey(key);

    try {
        server::Request request;
        request.type = server::RequestType::kExecuteScript;  // Temporarily use existing type
        request.data = {{"action", "set"}, {"key", fullKey}, {"value", value}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK) {
            // Update cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[fullKey] = value;
            }
            return true;
        }
    } catch (...) {}

    return false;
}

bool TeamStorage::removeItem(const std::string& key) {
    std::string fullKey = getFullKey(key);

    try {
        server::Request request;
        request.type = server::RequestType::kStopScript;  // Temporarily use existing type
        request.data = {{"action", "delete"}, {"key", fullKey}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK) {
            // Clear cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache.erase(fullKey);
            }
            return true;
        }
    } catch (...) {}

    return false;
}

void TeamStorage::clear() {
    try {
        server::Request request;
        request.type = server::RequestType::kStopScript;
        request.data = {{"action", "clear"}, {"scope", "team"}};

        auto future = m_client->sendSync(request);
        future.get();

        // Clear cache
        if (m_cacheEnabled) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cache.clear();
        }
    } catch (...) {}
}

bool TeamStorage::hasItem(const std::string& key) const {
    return getItem(key).has_value();
}

void TeamStorage::onChange(const std::string& key, ChangeCallback callback) {
    // TODO: Need to extend Client and Server to support subscription
    // Record callback for now, connect when subscribe is implemented
    (void)key;
    (void)callback;
}

void TeamStorage::broadcast(const std::string& event, const std::string& data) {
    try {
        server::Request request;
        request.type = server::RequestType::kExecuteScript;
        request.data = {
            {"action", "broadcast"},
            {"channel", "team:" + m_prefix},
            {"event", event},
            {"data", data}
        };

        m_client->send(request, [](const server::Response&) {
            // Send asynchronously, ignore callback
        });
    } catch (...) {}
}


// ========== ServerStorage Implementation ==========

ServerStorage::ServerStorage(std::shared_ptr<server::Client> client)
    : m_client(client) {
    if (!m_client) {
        throw std::invalid_argument("Client cannot be null");
    }
}

std::string ServerStorage::getFullKey(const std::string& key) const {
    if (m_prefix.empty()) {
        return "global:" + key;
    }
    return "global:" + m_prefix + ":" + key;
}

size_t ServerStorage::length() const {
    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "keys"}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK &&
            response.data.contains("result") && response.data["result"].is_array()) {
            return response.data["result"].size();
        }
    } catch (...) {}

    return 0;
}

std::vector<std::string> ServerStorage::keys() const {
    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "keys"}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK &&
            response.data.contains("result") && response.data["result"].is_array()) {
            return response.data["result"].get<std::vector<std::string>>();
        }
    } catch (...) {}

    return {};
}

std::optional<std::string> ServerStorage::getItem(const std::string& key) const {
    std::string fullKey = getFullKey(key);

    // Check cache first
    if (m_cacheEnabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(fullKey);
        if (it != m_cache.end()) {
            return it->second;
        }
    }

    try {
        server::Request request;
        request.type = server::RequestType::kGetStatus;
        request.data = {{"action", "get"}, {"key", fullKey}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK && response.data.contains("result")) {
            std::string value = response.data["result"].get<std::string>();

            // Update cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[fullKey] = value;
            }

            return value;
        }
    } catch (...) {}

    return std::nullopt;
}

bool ServerStorage::setItem(const std::string& key, const std::string& value) {
    std::string fullKey = getFullKey(key);

    try {
        server::Request request;
        request.type = server::RequestType::kExecuteScript;  // Temporarily use existing type
        request.data = {{"action", "set"}, {"key", fullKey}, {"value", value}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK) {
            // Update cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache[fullKey] = value;
            }
            return true;
        }
    } catch (...) {}

    return false;
}

bool ServerStorage::removeItem(const std::string& key) {
    std::string fullKey = getFullKey(key);

    try {
        server::Request request;
        request.type = server::RequestType::kStopScript;  // Temporarily use existing type
        request.data = {{"action", "delete"}, {"key", fullKey}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        auto response = future.get();

        if (response.code == server::ErrorCode::OK) {
            // Clear cache
            if (m_cacheEnabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cache.erase(fullKey);
            }
            return true;
        }
    } catch (...) {}

    return false;
}

void ServerStorage::clear() {
    try {
        server::Request request;
        request.type = server::RequestType::kStopScript;
        request.data = {{"action", "clear"}, {"scope", "global"}};

        auto future = m_client->sendSync(request);
        future.get();

        // Clear cache
        if (m_cacheEnabled) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cache.clear();
        }
    } catch (...) {}
}

bool ServerStorage::hasItem(const std::string& key) const {
    return getItem(key).has_value();
}

void ServerStorage::onChange(const std::string& key, ChangeCallback callback) {
    // TODO: Need to extend Client and Server to support subscription
    // Record callback for now, connect when subscribe is implemented
    (void)key;
    (void)callback;
}

void ServerStorage::broadcast(const std::string& event, const std::string& data) {
    try {
        server::Request request;
        request.type = server::RequestType::kExecuteScript;
        request.data = {
            {"action", "broadcast"},
            {"channel", "global"},
            {"event", event},
            {"data", data}
        };

        m_client->send(request, [](const server::Response&) {
            // Send asynchronously, ignore callback
        });
    } catch (...) {}
}


// ========== StorageFactory Remote Implementation ==========

std::unique_ptr<TeamStorage> StorageFactory::createTeam(
    std::shared_ptr<server::Client> client
) {
    return std::make_unique<TeamStorage>(client);
}

std::unique_ptr<ServerStorage> StorageFactory::createServer(
    std::shared_ptr<server::Client> client
) {
    return std::make_unique<ServerStorage>(client);
}

} // namespace wingman
