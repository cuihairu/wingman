#include "macro_module.hpp"
#include "wingman/recorder.hpp"

#include <atomic>
#include <memory>
#include <mutex>

namespace wingman {
namespace script {
namespace modules {

namespace {

// runtime 注入的实例（优先使用，与 RPC macro.* 共享）。
std::atomic<MacroRecorder*> g_recorder{nullptr};

// 未注入时的 lazy 默认实例（单脚本 CLI 等场景）。
std::unique_ptr<MacroRecorder> g_default;
std::mutex g_defaultMutex;

// 获取录制器引用。注入优先，否则 lazy 创建默认实例。
// 注意：MacroRecorder 的平台 hook 通过全局 g_instance 指针回调，
// 因此同时实际只有一个录制器能生效——单例语义与底层一致。
MacroRecorder& recorder() {
    MacroRecorder* r = g_recorder.load(std::memory_order_acquire);
    if (r) return *r;
    std::lock_guard<std::mutex> lock(g_defaultMutex);
    if (!g_default) g_default = std::make_unique<MacroRecorder>();
    return *g_default;
}

} // namespace

void setGlobalRecorder(MacroRecorder* recorder) {
    g_recorder.store(recorder, std::memory_order_release);
}

void cleanupMacroModule() {
    std::lock_guard<std::mutex> lock(g_defaultMutex);
    // 仅释放模块自有的默认实例；runtime 注入的实例生命周期由 runtime 管理。
    g_default.reset();
}

ModuleDescriptor createMacroModule() {
    ModuleDescriptor mod;
    mod.name = "macro";

    // start() -> bool（是否正在录制）
    mod.functions.push_back({"start", [](const std::vector<ScriptValue>&) -> ScriptValue {
        recorder().start();
        return ScriptValue::fromBool(recorder().isRecording());
    }, "() -> bool"});

    // stop() -> bool
    mod.functions.push_back({"stop", [](const std::vector<ScriptValue>&) -> ScriptValue {
        recorder().stop();
        return ScriptValue::fromBool(true);
    }, "() -> bool"});

    // pause() -> bool
    mod.functions.push_back({"pause", [](const std::vector<ScriptValue>&) -> ScriptValue {
        recorder().pause();
        return ScriptValue::fromBool(true);
    }, "() -> bool"});

    // resume() -> bool
    mod.functions.push_back({"resume", [](const std::vector<ScriptValue>&) -> ScriptValue {
        recorder().resume();
        return ScriptValue::fromBool(true);
    }, "() -> bool"});

    // clear() -> bool
    mod.functions.push_back({"clear", [](const std::vector<ScriptValue>&) -> ScriptValue {
        recorder().clear();
        return ScriptValue::fromBool(true);
    }, "() -> bool"});

    // isRecording() -> bool
    mod.functions.push_back({"isRecording", [](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(recorder().isRecording());
    }, "() -> bool"});

    // isPaused() -> bool
    mod.functions.push_back({"isPaused", [](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(recorder().isPaused());
    }, "() -> bool"});

    // getEventCount() -> int
    mod.functions.push_back({"getEventCount", [](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromInt(static_cast<int64_t>(recorder().getEventCount()));
    }, "() -> int"});

    // saveToLua(path) -> bool
    mod.functions.push_back({"saveToLua", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(recorder().saveToLua(args[0].asString()));
    }, "path:string -> bool"});

    // saveToJSON(path) -> bool
    mod.functions.push_back({"saveToJSON", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(recorder().saveToJSON(args[0].asString()));
    }, "path:string -> bool"});

    // loadFromJSON(path) -> bool
    mod.functions.push_back({"loadFromJSON", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(recorder().loadFromJSON(args[0].asString()));
    }, "path:string -> bool"});

    // playback(speed?, repeat?) -> bool
    mod.functions.push_back({"playback", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        const int speed = args.size() > 0 ? static_cast<int>(args[0].asInt(100)) : 100;
        const int repeat = args.size() > 1 ? static_cast<int>(args[1].asInt(1)) : 1;
        recorder().playback(speed, repeat);
        return ScriptValue::fromBool(true);
    }, "speed?:int=100, repeat?:int=1 -> bool"});

    // status() -> {recording:bool, paused:bool, eventCount:int}
    mod.functions.push_back({"status", [](const std::vector<ScriptValue>&) -> ScriptValue {
        auto& r = recorder();
        return ScriptValue::fromObject({
            {"recording", ScriptValue::fromBool(r.isRecording())},
            {"paused", ScriptValue::fromBool(r.isPaused())},
            {"eventCount", ScriptValue::fromInt(static_cast<int64_t>(r.getEventCount()))}
        });
    }, "() -> {recording:bool, paused:bool, eventCount:int}"});

    return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
