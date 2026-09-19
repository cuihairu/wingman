#pragma once

// Android 端 Lua 脚本执行器（A1，docs/android-agent-design.md §5.2）。
//
// 纯 C++ + sol2，不依赖任何 Android API：桌面环境直接编译并单测
// （libs/lua/tests/script_runner_test.cpp），Android NDK 构建同一份源码。
//
// 语义：
//  - 一次一个脚本：start() 时已有脚本在跑则拒绝；
//  - print/wingman.log 重定向到输出回调（经 AndroidAgent 以 agent.event
//    "script_output" {scriptId, message} 回传 server，与桌面字段一致）；
//  - wingman.sleep(ms) 分片睡眠并检查停止标志（协作中断点）；
//  - stop() 置停止标志，脚本线程内的指令计数 hook 在下一个检查点
//    luaL_error 强制中断（兜底无 sleep 的死循环）。

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace wingman::android {

class ScriptRunner {
public:
    // line 为脚本的一行输出（print 参数已按 Lua 标准以 tab 连接）
    using OutputCallback = std::function<void(const std::string& line)>;

    ScriptRunner() = default;
    ~ScriptRunner();

    ScriptRunner(const ScriptRunner&) = delete;
    ScriptRunner& operator=(const ScriptRunner&) = delete;

    // 异步执行：成功返回 true 并起执行线程；已有脚本在跑返回 false。
    // language 仅支持 "lua"（空串视为 lua，与 server 下发约定一致）。
    bool start(const std::string& executionId, const std::string& content,
               const std::string& language = "lua");

    // 请求停止当前脚本（幂等；无脚本在跑时为 no-op）
    void stop();

    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    // 当前/最近一次执行的脚本 ID（start 失败时不变）
    std::string executionId() const;

    void setOutputCallback(OutputCallback callback);

    // 同步执行入口：start() 的内部线程与单测共用。
    // stopFlag 置位后，sleep 检查点与指令 hook 均会中断脚本；
    // 返回脚本是否正常跑完（无 Lua 错误、未被停止）。
    bool runSync(const std::string& executionId, const std::string& content,
                 const std::string& language, std::atomic<bool>& stopFlag,
                 const OutputCallback& output);

private:
    void threadMain(std::string executionId, std::string content, std::string language);
    void emitLine(const std::string& line);

    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    std::thread worker_;
    mutable std::mutex executionMutex_;
    std::string executionId_;
    mutable std::mutex outputMutex_;
    OutputCallback output_;
};

} // namespace wingman::android
