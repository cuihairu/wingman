#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace wingman {

// JSON 值类型
enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

// JSON 值
class JsonValue {
public:
    JsonValue();
    JsonValue(nullptr_t);
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

    // 类型检查
    JsonType type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    // 获取值
    bool asBool() const;
    int asInt() const;
    int64_t asInt64() const;
    double asDouble() const;
    std::string asString() const;

    // 数组操作
    size_t size() const;
    JsonValue at(size_t index) const;
    void push(const JsonValue& value);

    // 对象操作
    std::vector<std::string> keys() const;
    JsonValue get(const std::string& key) const;
    void set(const std::string& key, const JsonValue& value);
    bool has(const std::string& key) const;

    // 序列化
    std::string dump(int indent = -1) const;
    static JsonValue parse(const std::string& json);

    // 便捷函数
    static JsonValue object() { return JsonValue::parse("{}"); }
    static JsonValue array() { return JsonValue::parse("[]"); }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace wingman
