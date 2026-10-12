// Lua 引擎基准：脚本往返是 workflow run()/任务执行体的公共路径，这里量
// 纯执行、跨语言函数调用与全模块注册后的模块分发（kv 胶水端到端）三档。
// 引擎创建/销毁成本单列（ScriptManager 每脚本一引擎的口径下是真实开销）。

#include "bench.hpp"

#include <wingman/lua/lua_script_engine.hpp>
#include <wingman/script/iscript_engine.hpp>
#include <wingman/script/module_registry.hpp>
#include <wingman/script/script_engine_factory.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace wingman;
using wingman::bench::Bench;

int main() {
    lua::registerLuaEngine();
    Bench b("lua");

    b.header("引擎生命周期");
    b.run("create + initialize + shutdown", [] {
        auto engine = script::ScriptEngineFactory::instance().createEngine("lua");
        engine->initialize();
        engine->shutdown();
    }, 200);

    auto engine = script::ScriptEngineFactory::instance().createEngine("lua");
    engine->initialize();

    b.header("纯脚本执行");
    b.run("executeString return 1+1", [&] {
        engine->executeString("return 1 + 1");
    }, 20000);

    engine->executeString(
        "function bench_add(a, b, c) return a + b + c end");

    b.header("跨语言函数调用");
    script::ScriptValue result;
    b.run("callFunction bench_add(3 args)", [&] {
        engine->callFunction("bench_add",
            {script::ScriptValue::fromInt(1), script::ScriptValue::fromInt(2),
             script::ScriptValue::fromInt(3)}, result);
    }, 20000);

    b.header("模块胶水端到端（全模块注册后）");
    script::modules::registerAllModules(*engine);
    b.run("wingman.kv.set via executeString", [&] {
        engine->executeString("wingman.kv.set('bench_key', 'bench_value')");
    }, 5000);
    b.run("wingman.kv.get via executeString", [&] {
        engine->executeString("wingman.kv.get('bench_key')");
    }, 5000);

    engine->shutdown();
    std::cout << "\ndone\n";
    return 0;
}
