#pragma once

#include <string>
#include <vector>
#include <memory>

namespace wingman::proto {

// ========== JSON 辅助函数 ==========

// 将 JsonData 转换为 C++ 值
class JsonValue {
public:
    static std::string parse(const std::string& json);
    static std::string serialize(const std::string& value);
};

// ========== Protobuf 辅助函数 ==========

// 创建 JSON 包装消息
template<typename T>
std::string wrapJson(const T& obj);

// 解析 JSON 包装消息
template<typename T>
T unwrapJson(const std::string& json);

} // namespace wingman::proto
