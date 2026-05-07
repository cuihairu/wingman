#include "wingman/proto/wrapper.hpp"
#include <nlohmann/json.hpp>

namespace wingman::proto {

std::string JsonValue::parse(const std::string& json) {
    return json;  // 直接返回，由调用方解析
}

std::string JsonValue::serialize(const std::string& value) {
    return value;  // 已经是 JSON 字符串
}

} // namespace wingman::proto
