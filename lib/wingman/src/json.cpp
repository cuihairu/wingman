#include "wingman/json.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace wingman {

using json = nlohmann::json;

// JsonValue implementation
class JsonValue::Impl {
public:
    json data;

    Impl() : data(nullptr) {}
    Impl(const json& j) : data(j) {}
};

JsonValue::JsonValue() : m_impl(std::make_unique<Impl>()) {}
JsonValue::JsonValue(std::nullptr_t) : m_impl(std::make_unique<Impl>(nullptr)) {}
JsonValue::JsonValue(bool value) : m_impl(std::make_unique<Impl>(value)) {}
JsonValue::JsonValue(int value) : m_impl(std::make_unique<Impl>(value)) {}
JsonValue::JsonValue(int64_t value) : m_impl(std::make_unique<Impl>(value)) {}
JsonValue::JsonValue(double value) : m_impl(std::make_unique<Impl>(value)) {}
JsonValue::JsonValue(const char* value) : m_impl(std::make_unique<Impl>(value)) {}
JsonValue::JsonValue(const std::string& value) : m_impl(std::make_unique<Impl>(value)) {}

JsonValue::JsonValue(const JsonValue& other) : m_impl(std::make_unique<Impl>(other.m_impl->data)) {}

JsonValue& JsonValue::operator=(const JsonValue& other) {
    if (this != &other) {
        m_impl = std::make_unique<Impl>(other.m_impl->data);
    }
    return *this;
}

JsonType JsonValue::type() const {
    if (m_impl->data.is_null()) return JsonType::Null;
    if (m_impl->data.is_boolean()) return JsonType::Boolean;
    if (m_impl->data.is_number()) return JsonType::Number;
    if (m_impl->data.is_string()) return JsonType::String;
    if (m_impl->data.is_array()) return JsonType::Array;
    if (m_impl->data.is_object()) return JsonType::Object;
    return JsonType::Null;
}

bool JsonValue::isNull() const { return m_impl->data.is_null(); }
bool JsonValue::isBool() const { return m_impl->data.is_boolean(); }
bool JsonValue::isNumber() const { return m_impl->data.is_number(); }
bool JsonValue::isString() const { return m_impl->data.is_string(); }
bool JsonValue::isArray() const { return m_impl->data.is_array(); }
bool JsonValue::isObject() const { return m_impl->data.is_object(); }

bool JsonValue::asBool() const {
    if (m_impl->data.is_boolean()) {
        return m_impl->data.get<bool>();
    }
    if (m_impl->data.is_number()) {
        return m_impl->data.get<int>() != 0;
    }
    if (m_impl->data.is_string()) {
        return !m_impl->data.get<std::string>().empty();
    }
    return false;
}
int JsonValue::asInt() const { return m_impl->data.get<int>(); }
int64_t JsonValue::asInt64() const { return m_impl->data.get<int64_t>(); }
double JsonValue::asDouble() const { return m_impl->data.get<double>(); }
std::string JsonValue::asString() const { return m_impl->data.get<std::string>(); }

size_t JsonValue::size() const {
    if (m_impl->data.is_array()) {
        return m_impl->data.size();
    }
    if (m_impl->data.is_object()) {
        return m_impl->data.size();
    }
    return 0;
}

JsonValue JsonValue::at(size_t index) const {
    if (index >= m_impl->data.size()) {
        throw std::out_of_range("Index out of range");
    }
    JsonValue result;
    result.m_impl->data = m_impl->data[index];
    return result;
}

void JsonValue::push(const JsonValue& value) {
    if (!m_impl->data.is_array()) {
        m_impl->data = json::array();
    }
    m_impl->data.push_back(value.m_impl->data);
}

std::vector<std::string> JsonValue::keys() const {
    std::vector<std::string> result;
    if (m_impl->data.is_object()) {
        for (auto it = m_impl->data.begin(); it != m_impl->data.end(); ++it) {
            result.push_back(it.key());
        }
    }
    return result;
}

JsonValue JsonValue::get(const std::string& key) const {
    JsonValue result;
    if (m_impl->data.is_object() && m_impl->data.contains(key)) {
        result.m_impl->data = m_impl->data[key];
    }
    return result;
}

void JsonValue::set(const std::string& key, const JsonValue& value) {
    if (!m_impl->data.is_object()) {
        m_impl->data = json::object();
    }
    m_impl->data[key] = value.m_impl->data;
}

bool JsonValue::has(const std::string& key) const {
    return m_impl->data.is_object() && m_impl->data.contains(key);
}

std::string JsonValue::dump(int indent) const {
    if (indent < 0) {
        return m_impl->data.dump();
    }
    return m_impl->data.dump(indent);
}

JsonValue JsonValue::parse(const std::string& jsonStr) {
    JsonValue result;
    try {
        result.m_impl->data = json::parse(jsonStr);
    } catch (const json::parse_error&) {
        result.m_impl->data = nullptr;
    }
    return result;
}

JsonValue::~JsonValue() = default;

} // namespace wingman
