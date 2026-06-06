#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace wingman::rpc {

class RpcDispatcher {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json&)>;

    void registerHandler(const std::string& method, Handler handler);
    std::string dispatch(const std::string& rawMessage);

private:
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace wingman::rpc
