#pragma once

#include "wingman/storage/storage.hpp"

#include <unordered_map>
#include <mutex>

namespace wingman {

// SessionStorage: In-memory storage, data cleared on process exit
// Similar to browser sessionStorage, suitable for temporary session data
class SessionStorage : public IStorage {
public:
    SessionStorage() = default;
    ~SessionStorage() override = default;

    // Non-copyable
    SessionStorage(const SessionStorage&) = delete;
    SessionStorage& operator=(const SessionStorage&) = delete;

    // IStorage interface implementation
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
