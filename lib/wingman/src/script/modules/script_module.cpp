#include "script_module.hpp"
#include "wingman/script_manager.hpp"

#include <atomic>
#include <string>
#include <unordered_map>

namespace {
// runtime 注入的 ScriptManager 指针（懒查询，规避循环依赖）。
std::atomic<wingman::ScriptManager*> g_scriptManager{nullptr};
} // namespace

namespace wingman {
namespace script {

// 实现声明于 wingman/script/runtime_injections.hpp
void setScriptManager(ScriptManager* sm) {
    g_scriptManager.store(sm, std::memory_order_release);
}

namespace modules {

namespace {

wingman::ScriptManager* mgr() {
    return g_scriptManager.load(std::memory_order_acquire);
}

const char* stateStr(ScriptState s) {
    switch (s) {
        case ScriptState::unloaded:  return "unloaded";
        case ScriptState::loaded:    return "loaded";
        case ScriptState::starting:  return "starting";
        case ScriptState::running:   return "running";
        case ScriptState::paused:    return "paused";
        case ScriptState::stopping:  return "stopping";
        case ScriptState::completed: return "completed";
        case ScriptState::error:     return "error";
    }
    return "unknown";
}

ScriptValue infoToValue(const ScriptInfo& info) {
    std::unordered_map<std::string, ScriptValue> obj;
    obj["name"] = ScriptValue::fromString(info.config.name);
    obj["path"] = ScriptValue::fromString(info.config.path);
    obj["state"] = ScriptValue::fromString(stateStr(info.state));
    obj["language"] = ScriptValue::fromString(info.language);
    if (!info.lastError.empty()) {
        obj["lastError"] = ScriptValue::fromString(info.lastError);
    }
    return ScriptValue::fromObject(std::move(obj));
}

} // namespace

ModuleDescriptor createScriptModule() {
    ModuleDescriptor mod;
    mod.name = "script";

    // list() -> [{name, path, state, language, lastError?}]
    mod.functions.push_back({"list", [](const std::vector<ScriptValue>&) -> ScriptValue {
        auto* m = mgr();
        if (!m) return ScriptValue::fromArray({});
        const auto infos = m->getAllScriptInfos();
        std::vector<ScriptValue> arr;
        arr.reserve(infos.size());
        for (const auto& info : infos) {
            arr.push_back(infoToValue(info));
        }
        return ScriptValue::fromArray(std::move(arr));
    }, "() -> array"});

    // getState(name) -> string
    mod.functions.push_back({"getState", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromString("");
        auto info = m->getScriptInfo(args[0].asString());
        return ScriptValue::fromString(info ? stateStr(info->state) : "unknown");
    }, "name:string -> string"});

    // isRunning(name) -> bool
    mod.functions.push_back({"isRunning", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        auto info = m->getScriptInfo(args[0].asString());
        return ScriptValue::fromBool(info && info->state == ScriptState::running);
    }, "name:string -> bool"});

    // has(name) -> bool
    mod.functions.push_back({"has", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(m->hasScript(args[0].asString()));
    }, "name:string -> bool"});

    // reload(name) -> bool
    mod.functions.push_back({"reload", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(m->reloadScript(args[0].asString()));
    }, "name:string -> bool"});

    // run(name) -> bool
    mod.functions.push_back({"run", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(m->runScript(args[0].asString()));
    }, "name:string -> bool"});

    // stop(name) -> bool
    mod.functions.push_back({"stop", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        return ScriptValue::fromBool(m->stopScript(args[0].asString()));
    }, "name:string -> bool"});

    // setEnv(name, key, value) -> bool
    // 注：ScriptManager::setEnv 当前为全局环境（非 per-script）；name 参数保留以兼容调用形态。
    mod.functions.push_back({"setEnv", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.size() < 3) return ScriptValue::fromBool(false);
        m->setEnv(args[1].asString(), args[2].asString());
        return ScriptValue::fromBool(true);
    }, "name:string, key:string, value:string -> bool"});

    // getEnv(name, key) -> string
    mod.functions.push_back({"getEnv", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.size() < 2) return ScriptValue::fromString("");
        return ScriptValue::fromString(m->getEnv(args[1].asString()));
    }, "name:string, key:string -> string"});

    // setConfig(key, value) -> bool
    mod.functions.push_back({"setConfig", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.size() < 2) return ScriptValue::fromBool(false);
        m->setConfig(args[0].asString(), args[1].asString());
        return ScriptValue::fromBool(true);
    }, "key:string, value:string -> bool"});

    // getConfig(key) -> string
    mod.functions.push_back({"getConfig", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromString("");
        return ScriptValue::fromString(m->getConfig(args[0].asString()));
    }, "key:string -> string"});

    // setHotReload(enabled) -> bool
    mod.functions.push_back({"setHotReload", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.empty()) return ScriptValue::fromBool(false);
        const bool enabled = args[0].asBool();
        m->setGlobalAutoReload(enabled);
        if (enabled) m->startHotReload(); else m->stopHotReload();
        return ScriptValue::fromBool(true);
    }, "enabled:bool -> bool"});

    // load(name, path, config?) -> bool
    mod.functions.push_back({"load", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* m = mgr();
        if (!m || args.size() < 2) return ScriptValue::fromBool(false);
        ScriptConfig cfg;
        cfg.name = args[0].asString();
        cfg.path = args[1].asString();
        if (args.size() > 2 && args[2].isObject()) {
            const ScriptValue& c = args[2];
            if (const ScriptValue* v = c.get("autoReload")) cfg.autoReload = v->asBool();
            if (const ScriptValue* v = c.get("sandboxed")) cfg.sandboxed = v->asBool();
            if (const ScriptValue* v = c.get("timeoutMs")) cfg.timeoutMs = static_cast<int>(v->asInt(30000));
        }
        return ScriptValue::fromBool(m->loadScript(cfg.name, cfg.path, cfg));
    }, "name:string, path:string, config?:{autoReload?:bool, sandboxed?:bool, timeoutMs?:int} -> bool"});

    return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
