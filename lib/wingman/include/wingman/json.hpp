#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace wingman {

// JSON value type
enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

// JSON value
class JsonValue {
public:
    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(int value);
    JsonValue(int64_t value);
    JsonValue(double value);
    JsonValue(const char* value);
    JsonValue(const std::string& value);

    // Copy constructor and assignment (needed for unique_ptr member)
    JsonValue(const JsonValue& other);
    JsonValue& operator=(const JsonValue& other);

    ~JsonValue();

    // Type check
    JsonType type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    // Get value
    bool asBool() const;
    int asInt() const;
    int64_t asInt64() const;
    double asDouble() const;
    std::string asString() const;

    // Array operations
    size_t size() const;
    JsonValue at(size_t index) const;
    void push(const JsonValue& value);

    // Object operations
    std::vector<std::string> keys() const;
    JsonValue get(const std::string& key) const;
    void set(const std::string& key, const JsonValue& value);
    bool has(const std::string& key) const;

    // Serialization
    std::string dump(int indent = -1) const;
    static JsonValue parse(const std::string& json);

    // Convenience functions
    static JsonValue object() { return JsonValue::parse("{}"); }
    static JsonValue array() { return JsonValue::parse("[]"); }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace wingman
