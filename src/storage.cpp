#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"
#include "wingman/storage/storage_all.hpp"

#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace wingman {

// ========== SessionStorage Implementation ==========

size_t SessionStorage::length() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.size();
}

std::vector<std::string> SessionStorage::keys() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> result;
    result.reserve(m_data.size());

    for (const auto& pair : m_data) {
        result.push_back(pair.first);
    }

    return result;
}

std::optional<std::string> SessionStorage::getItem(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool SessionStorage::setItem(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data[key] = value;
    return true;
}

bool SessionStorage::removeItem(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_data.find(key);
    if (it != m_data.end()) {
        m_data.erase(it);
        return true;
    }

    return false;
}

void SessionStorage::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.clear();
}

bool SessionStorage::hasItem(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.find(key) != m_data.end();
}


// ========== LocalStorage Implementation ==========

LocalStorage::LocalStorage(const std::filesystem::path& storageDir)
    : m_storagePath(storageDir) {
    // 确保存储目录存在
    std::filesystem::create_directories(m_storagePath);

    // 自动加载已有数据
    load();
}

LocalStorage::~LocalStorage() {
    // 析构时自动保存
    save();
}

std::string LocalStorage::getFullKey(const std::string& key) const {
    if (m_namespace.empty()) {
        return key;
    }
    return m_namespace + ":" + key;
}

std::filesystem::path LocalStorage::getStorageFile() const {
    if (m_namespace.empty()) {
        return m_storagePath / "storage.json";
    }
    return m_storagePath / (m_namespace + ".json");
}

size_t LocalStorage::length() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.size();
}

std::vector<std::string> LocalStorage::keys() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> result;
    result.reserve(m_data.size());

    for (const auto& pair : m_data) {
        result.push_back(pair.first);
    }

    return result;
}

std::optional<std::string> LocalStorage::getItem(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string fullKey = getFullKey(key);
    auto it = m_data.find(fullKey);

    if (it != m_data.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool LocalStorage::setItem(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string fullKey = getFullKey(key);
    m_data[fullKey] = value;

    return save();
}

bool LocalStorage::removeItem(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string fullKey = getFullKey(key);
    auto it = m_data.find(fullKey);

    if (it != m_data.end()) {
        m_data.erase(it);
        return save();
    }

    return false;
}

void LocalStorage::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.clear();
    save();
}

bool LocalStorage::hasItem(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string fullKey = getFullKey(key);
    return m_data.find(fullKey) != m_data.end();
}

bool LocalStorage::save() {
    try {
        std::filesystem::path filePath = getStorageFile();

        // 使用 nlohmann/json 保存
        nlohmann::json j = nlohmann::json::object();

        for (const auto& pair : m_data) {
            j[pair.first] = pair.second;
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(2);  // 缩进 2 格，方便阅读
        return true;

    } catch (...) {
        return false;
    }
}

bool LocalStorage::load() {
    try {
        std::filesystem::path filePath = getStorageFile();

        if (!std::filesystem::exists(filePath)) {
            return true;  // 文件不存在是正常情况
        }

        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto it = j.begin(); it != j.end(); ++it) {
            m_data[it.key()] = it.value().get<std::string>();
        }

        return true;

    } catch (...) {
        return false;
    }
}


// ========== StorageFactory Implementation ==========

std::unique_ptr<SessionStorage> StorageFactory::createSession() {
    return std::make_unique<SessionStorage>();
}

std::unique_ptr<LocalStorage> StorageFactory::createLocal(
    const std::filesystem::path& storageDir
) {
    return std::make_unique<LocalStorage>(storageDir);
}

} // namespace wingman
