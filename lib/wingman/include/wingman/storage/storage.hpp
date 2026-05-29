#pragma once

#include <string>
#include <vector>
#include <optional>

namespace wingman {

// Storage interface (similar to browser Web Storage API)
class IStorage {
public:
    virtual ~IStorage() = default;

    // Get item count
    virtual size_t length() const = 0;

    // Get all keys
    virtual std::vector<std::string> keys() const = 0;

    // Get value (returns nullopt if not found)
    virtual std::optional<std::string> getItem(const std::string& key) const = 0;

    // Set value (returns whether successful)
    virtual bool setItem(const std::string& key, const std::string& value) = 0;

    // Remove value (returns whether it existed)
    virtual bool removeItem(const std::string& key) = 0;

    // Clear all data
    virtual void clear() = 0;

    // Check if key exists
    virtual bool hasItem(const std::string& key) const = 0;
};

} // namespace wingman
