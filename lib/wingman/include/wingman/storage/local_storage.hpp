#pragma once

#include "wingman/storage/storage.hpp"

#include <unordered_map>
#include <mutex>
#include <string>
#include <filesystem>

namespace wingman {

// LocalStorage: File-based persistent storage, data preserved across process restarts
// Similar to browser localStorage, suitable for local config and cache
class LocalStorage : public IStorage {
public:
    // Constructor, specifies storage directory
    explicit LocalStorage(const std::filesystem::path& storageDir = "storage");
    ~LocalStorage() override;

    // Non-copyable
    LocalStorage(const LocalStorage&) = delete;
    LocalStorage& operator=(const LocalStorage&) = delete;

    // IStorage interface implementation
    size_t length() const override;

    std::vector<std::string> keys() const override;

    std::optional<std::string> getItem(const std::string& key) const override;

    bool setItem(const std::string& key, const std::string& value) override;

    bool removeItem(const std::string& key) override;

    void clear() override;

    bool hasItem(const std::string& key) const override;

    // Additional methods

    // Save to disk immediately
    bool save();

    // Reload from disk
    bool load();

    // Get storage path
    const std::filesystem::path& getStoragePath() const { return m_storagePath; }

    // Set namespace (for isolating data from different modules)
    void setNamespace(const std::string& ns) { m_namespace = ns; }
    const std::string& getNamespace() const { return m_namespace; }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_data;
    std::filesystem::path m_storagePath;
    std::string m_namespace;

    // Get full key with namespace
    std::string getFullKey(const std::string& key) const;

    // Get storage file path
    std::filesystem::path getStorageFile() const;
};

} // namespace wingman
