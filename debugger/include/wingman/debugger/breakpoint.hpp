#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace wingman {

// 断点类型
enum class BreakpointType {
    Line,           // 行断点
    Condition       // 条件断点
};

// 断点状态
enum class BreakpointState {
    Enabled,
    Disabled,
    Hit
};

// 断点信息
struct Breakpoint {
    size_t id;                          // 断点ID
    std::string file;                   // 文件路径
    int line;                           // 行号
    BreakpointType type;                // 断点类型
    BreakpointState state;              // 断点状态
    std::string condition;              // 条件表达式（条件断点）
    int hitCount;                       // 命中次数
    int ignoreCount;                    // 忽略次数（前N次不触发）

    Breakpoint() : id(0), line(0), type(BreakpointType::Line),
                   state(BreakpointState::Enabled), hitCount(0), ignoreCount(0) {}
};

// 断点管理器
class BreakpointManager {
public:
    BreakpointManager();
    ~BreakpointManager() = default;

    // 添加断点
    size_t addBreakpoint(const std::string& file, int line,
                        BreakpointType type = BreakpointType::Line,
                        const std::string& condition = "");

    // 移除断点
    bool removeBreakpoint(size_t id);
    void removeBreakpointsForFile(const std::string& file);
    void clearAll();

    // 启用/禁用断点
    bool enableBreakpoint(size_t id);
    bool disableBreakpoint(size_t id);

    // 查找断点
    std::optional<Breakpoint*> findBreakpoint(const std::string& file, int line);
    std::vector<Breakpoint*> getBreakpointsForFile(const std::string& file);
    std::vector<Breakpoint> getAllBreakpoints() const;

    // 检查是否应该在此处停止
    bool shouldBreak(const std::string& file, int line);

    // 命中计数
    void incrementHitCount(size_t id);
    int getHitCount(size_t id) const;

private:
    std::vector<Breakpoint> breakpoints_;
    size_t nextId_;

    // 文件到断点的快速索引
    std::unordered_map<std::string, std::vector<size_t>> fileIndex_;
};

} // namespace wingman
