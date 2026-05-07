// JSON-only wrapper for wingman-proto (used when Protobuf is not available)
#include "wingman/proto/proto_wrapper.hpp"
#include <nlohmann/json.hpp>

namespace wingman::proto {

// ProtoWrapper 实现（JSON 版本）
class ProtoWrapper::Impl {
public:
    nlohmann::json data;
};

ProtoWrapper::ProtoWrapper() : impl_(std::make_unique<Impl>()) {}

ProtoWrapper::~ProtoWrapper() = default;

// 序列化为 JSON
std::string ProtoWrapper::toJson() const {
    return impl_->data.dump();
}

// 从 JSON 反序列化
bool ProtoWrapper::fromJson(const std::string& jsonStr) {
    try {
        impl_->data = nlohmann::json::parse(jsonStr);
        return true;
    } catch (...) {
        return false;
    }
}

// 序列化为二进制（JSON bytes）
std::vector<uint8_t> ProtoWrapper::toBytes() const {
    auto jsonStr = toJson();
    return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
}

// 从二进制反序列化（JSON bytes）
bool ProtoWrapper::fromBytes(const std::vector<uint8_t>& bytes) {
    try {
        std::string jsonStr(bytes.begin(), bytes.end());
        return fromJson(jsonStr);
    } catch (...) {
        return false;
    }
}

} // namespace wingman::proto
