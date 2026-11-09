#pragma once

#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/json.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/human.hpp"
#include "wingman/ui_automation.hpp"
#include <algorithm>
#include <cstdio>
#include <set>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// Helper: find a function by module name and function name
inline ModuleDescriptor::FunctionEntry findFunction(const std::string& moduleName, const std::string& funcName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (mod.name == moduleName) {
            for (const auto& fn : mod.functions) {
                if (fn.name == funcName) return fn;
            }
        }
    }
    return {};
}

inline ModuleDescriptor::FunctionEntry findFunction(const std::string& funcName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        for (const auto& fn : mod.functions) {
            if (fn.name == funcName) return fn;
        }
    }
    return {};
}

inline bool result_is_string_or_null(const ScriptValue& v) {
    return v.isString() || v.isNull();
}

// Plan 5: 配置修改测试的全局状态守卫，测试结束自动恢复 Human mouse/keyboard 配置
struct HumanConfigGuard {
    HumanMouseConfig mc = Human::mouse().getConfig();
    HumanKeyboardConfig kc = Human::keyboard().getConfig();
    ~HumanConfigGuard() {
        Human::setMouseConfig(mc);
        Human::setKeyboardConfig(kc);
    }
};

inline bool hasModule(const std::string& moduleName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (mod.name == moduleName) return true;
    }
    return false;
}
