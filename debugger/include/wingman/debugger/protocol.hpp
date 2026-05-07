#pragma once

#include "wingman/debugger/breakpoint.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace wingman {

// 调试状态
enum class DebugState {
    Stopped,       // 停止在断点或步进
    Running,       // 正在运行
    Paused,        // 暂停
    Terminated     // 已终止
};

// 堆栈帧
struct StackFrame {
    size_t id;
    std::string name;
    std::string source;
    int line;
    int column;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["source"] = source;
        j["line"] = line;
        j["column"] = column;
        return j;
    }
};

// 变量信息
struct Variable {
    std::string name;
    std::string value;
    std::string type;
    size_t variablesReference;  // 用于嵌套结构（表/数组）
    bool indexed;               // 是否有索引

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["name"] = name;
        j["value"] = value;
        j["type"] = type;
        j["variablesReference"] = variablesReference;
        j["indexed"] = indexed;
        return j;
    }
};

// 作用域
enum class ScopeType {
    Locals,
    Globals,
    Upvalues
};

struct Scope {
    std::string name;
    size_t variablesReference;
    ScopeType type;
    bool expensive;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["name"] = name;
        j["variablesReference"] = variablesReference;
        j["type"] = static_cast<int>(type);
        j["expensive"] = expensive;
        return j;
    }
};

// 协议辅助函数
std::string debugStateToString(DebugState state);

} // namespace wingman
